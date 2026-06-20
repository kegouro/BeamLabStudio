#include "ui/views/BioSimView.h"

#include "ui/qt/presenters/BioSimPresenter.h"

#include "biosim/core/BioSimConfig.h"
#include "biosim/core/BioSimResult.h"
#include "biosim/geometry/BioSlab.h"
#include "biosim/ui/qt/BioViewport3D.h"
#include "biosim/ui/qt/EnergyScaleBar.h"
#include "biosim/ui/qt/MaterialEditorDialog.h"
#include "biosim/ui/qt/SlabEditor3D.h"
#include "biosim/ui/qt/SOBPWidget.h"

#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QSplitter>
#include <QTabWidget>
#include <QToolBar>
#include <QVBoxLayout>

namespace beamlab::ui {

// The viewport / slab editor / scale bar / material dialog all live in
// namespace beamlab::biosim; pull them in so the unqualified names below
// (and the QObject signal references in connect) resolve.
using namespace beamlab::biosim;

BioSimView::BioSimView(BioSimPresenter* presenter, QWidget* parent)
    : QWidget(parent)
    , presenter_(presenter)
{
    setupLayout();
    connectSignals();
}

void BioSimView::setupLayout()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // --- Toolbar: CSV | Run | Stop | Materials ---
    auto* toolbar = new QToolBar(this);
    toolbar->setMovable(false);

    auto* csvBtn = new QPushButton(QStringLiteral("CSV…"), toolbar);
    csvBtn->setToolTip(QStringLiteral("Select Geant4 energy_step_profile.csv"));
    toolbar->addWidget(csvBtn);

    csvLabel_ = new QLabel(QStringLiteral("(no file)"), toolbar);
    csvLabel_->setStyleSheet(QStringLiteral("color: #aaa; font-size: 9px; margin: 0 6px;"));
    csvLabel_->setMaximumWidth(220);
    toolbar->addWidget(csvLabel_);

    toolbar->addSeparator();

    runBtn_  = new QPushButton(QStringLiteral("▶ Run"),  toolbar);
    stopBtn_ = new QPushButton(QStringLiteral("■ Stop"), toolbar);
    stopBtn_->setEnabled(false);
    toolbar->addWidget(runBtn_);
    toolbar->addWidget(stopBtn_);

    toolbar->addSeparator();

    materialsBtn_ = new QPushButton(QStringLiteral("Materials…"), toolbar);
    toolbar->addWidget(materialsBtn_);

    root->addWidget(toolbar);

    // --- Tab widget: [Simulación 3D] | [SOBP] --------------------------------
    biosimTabs_ = new QTabWidget(this);
    biosimTabs_->setDocumentMode(true);

    // --- Tab 0: Simulación 3D ------------------------------------------------
    auto* simPage = new QWidget(biosimTabs_);

    // Main splitter (horizontal): [viewport + scalebar] | (future panels)
    horizontalSplitter_ = new QSplitter(Qt::Horizontal, simPage);

    // Vertical splitter on the left: 3D scene over slab editor.
    verticalSplitter_ = new QSplitter(Qt::Vertical, horizontalSplitter_);

    auto* sceneRow = new QWidget(verticalSplitter_);
    auto* sceneLayout = new QHBoxLayout(sceneRow);
    sceneLayout->setContentsMargins(0, 0, 0, 0);
    sceneLayout->setSpacing(2);

    viewport3D_  = new BioViewport3D(sceneRow);
    energyScale_ = new EnergyScaleBar(sceneRow);
    sceneLayout->addWidget(viewport3D_, 1);
    sceneLayout->addWidget(energyScale_, 0);
    verticalSplitter_->addWidget(sceneRow);

    slabEditor_ = new SlabEditor3D(verticalSplitter_);
    slabEditor_->setMaximumHeight(200);
    verticalSplitter_->addWidget(slabEditor_);
    verticalSplitter_->setStretchFactor(0, 3);
    verticalSplitter_->setStretchFactor(1, 1);

    horizontalSplitter_->addWidget(verticalSplitter_);

    auto* simPageLayout = new QVBoxLayout(simPage);
    simPageLayout->setContentsMargins(0, 0, 0, 0);
    simPageLayout->setSpacing(0);
    simPageLayout->addWidget(horizontalSplitter_, 1);

    biosimTabs_->addTab(simPage, QStringLiteral("Simulación 3D"));

    // --- Tab 1: SOBP ---------------------------------------------------------
    // ponytail: SOBPWidget contains its own SOBPCalculator (pure C++, no Qt).
    //   techo: single-threaded compute; no async needed for N≤30 peaks.
    //   upgrade: QFuture if N>30 or sub-mm resolution is requested.
    sobpWidget_ = new SOBPWidget(biosimTabs_);
    biosimTabs_->addTab(sobpWidget_, QStringLiteral("SOBP"));

    root->addWidget(biosimTabs_, 1);

    // --- Status bar ---
    auto* statusRow = new QHBoxLayout();
    statusRow->setContentsMargins(6, 2, 6, 2);
    statusLabel_ = new QLabel(QStringLiteral("Ready"), this);
    statusLabel_->setStyleSheet(QStringLiteral("color: #aaa; font-size: 9px;"));
    progressBar_ = new QProgressBar(this);
    progressBar_->setMaximumWidth(200);
    progressBar_->setMaximumHeight(14);
    progressBar_->setRange(0, 100);
    progressBar_->setValue(0);
    progressBar_->hide();
    statusRow->addWidget(statusLabel_, 1);
    statusRow->addWidget(progressBar_, 0);
    root->addLayout(statusRow);

    // CSV browse wired here (local widget, not stored as a member).
    connect(csvBtn, &QPushButton::clicked, this, &BioSimView::onBrowseCsv);
}

void BioSimView::connectSignals()
{
    connect(runBtn_,       &QPushButton::clicked, this, &BioSimView::onRun);
    connect(stopBtn_,      &QPushButton::clicked, this, &BioSimView::onStop);
    connect(materialsBtn_, &QPushButton::clicked, this, &BioSimView::onOpenMaterialEditor);

    // Slab edits update the 3D scene immediately.
    connect(slabEditor_, &SlabEditor3D::slabsChanged,
            this, [this](const std::vector<beamlab::biosim::BioSlab>& slabs) {
                viewport3D_->setSlabs(slabs);
            });

    if (presenter_) {
        connect(presenter_, &BioSimPresenter::simulationProgress,
                this, &BioSimView::onSimulationProgress);
        connect(presenter_, &BioSimPresenter::simulationCompleted,
                this, &BioSimView::onSimulationCompleted);
    }
}

void BioSimView::setCsvPath(const QString& path)
{
    csvPath_ = path;
    if (csvLabel_) {
        csvLabel_->setText(path.section('/', -1)); // basename
        csvLabel_->setToolTip(path);
    }
}

void BioSimView::onBrowseCsv()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("Select energy_step_profile.csv"),
        QString{},
        QStringLiteral("CSV files (*.csv);;All files (*)"));
    if (!path.isEmpty()) setCsvPath(path);
}

void BioSimView::onRun()
{
    if (!presenter_) return;
    if (running_) return;
    if (csvPath_.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("No file"),
                             QStringLiteral("Please select a CSV file first."));
        return;
    }

    running_ = true;
    runBtn_->setEnabled(false);
    stopBtn_->setEnabled(true);
    progressBar_->setValue(0);
    progressBar_->show();
    statusLabel_->setText(QStringLiteral("Running simulation…"));

    beamlab::biosim::BioSimConfig cfg;
    cfg.energy_step_csv_path = csvPath_.toStdString();
    cfg.slabs                = slabEditor_->slabs();

    presenter_->runSimulation(cfg);
}

void BioSimView::onStop()
{
    if (!presenter_) return;
    presenter_->stopSimulation();
    running_ = false;
    runBtn_->setEnabled(true);
    stopBtn_->setEnabled(false);
    progressBar_->hide();
    statusLabel_->setText(QStringLiteral("Stopped."));
}

void BioSimView::onSimulationProgress(int percent)
{
    progressBar_->setValue(percent);
}

void BioSimView::onSimulationCompleted(
    std::shared_ptr<beamlab::biosim::BioSimResult> result)
{
    running_ = false;
    runBtn_->setEnabled(true);
    stopBtn_->setEnabled(false);
    progressBar_->hide();

    if (!result || !result->valid) {
        const QString msg = result
            ? QString::fromStdString(result->message)
            : QStringLiteral("Simulation produced no result.");
        statusLabel_->setText(QStringLiteral("Error: ") + msg);
        return;
    }

    viewport3D_->setResult(*result);
    viewport3D_->resetCamera();

    statusLabel_->setText(QString("Done — %1 tracks, %2 steps")
        .arg(result->total_tracks)
        .arg(result->total_steps));
}

void BioSimView::onOpenMaterialEditor()
{
    MaterialEditorDialog dlg(this);
    dlg.exec();
}

} // namespace beamlab::ui
