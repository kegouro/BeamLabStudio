#include "biosim/ui/qt/BioSimWidget.h"
#include "biosim/core/BioSimRunner.h"
#include "biosim/ui/qt/MaterialEditorDialog.h"
#include "biosim/ui/qt/PhantomPresetPanel.h"

#include <QComboBox>
#include <QFileDialog>
#include <QFuture>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QSplitter>
#include <QToolBar>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

namespace beamlab::biosim {

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

BioSimWidget::BioSimWidget(QWidget* parent)
    : QWidget(parent)
{
    buildLayout();

    watcher_ = new QFutureWatcher<BioSimResult>(this);
    connect(watcher_, &QFutureWatcher<BioSimResult>::finished,
            this, &BioSimWidget::onSimFinished);
}

void BioSimWidget::buildLayout()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // --- Toolbar ---
    auto* toolbar = new QToolBar(this);
    toolbar->setIconSize(QSize(16, 16));
    toolbar->setMovable(false);
    buildToolbar(toolbar);
    root->addWidget(toolbar);

    // --- Main splitter (horizontal: viewport+scalebar | inspector) ---
    auto* h_split = new QSplitter(Qt::Horizontal, this);

    // Left: vertical splitter (3D view | slab editor)
    auto* v_split = new QSplitter(Qt::Vertical, h_split);

    // 3D viewport + scale bar in a horizontal row.
    auto* scene_row = new QWidget(v_split);
    auto* scene_layout = new QHBoxLayout(scene_row);
    scene_layout->setContentsMargins(0, 0, 0, 0);
    scene_layout->setSpacing(2);

    viewport_  = new BioViewport3D(scene_row);
    scale_bar_ = new EnergyScaleBar(scene_row);
    scene_layout->addWidget(viewport_, 1);
    scene_layout->addWidget(scale_bar_, 0);
    v_split->addWidget(scene_row);

    // Slab editor (bottom panel).
    slab_editor_ = new SlabEditor3D(v_split);
    slab_editor_->setMaximumHeight(200);
    v_split->addWidget(slab_editor_);
    v_split->setStretchFactor(0, 3);
    v_split->setStretchFactor(1, 1);

    h_split->addWidget(v_split);

    // Right: inspector panel.
    inspector_ = new TrajectoryInspectorPanel(h_split);
    inspector_->setMaximumWidth(340);
    inspector_->setMinimumWidth(220);
    h_split->addWidget(inspector_);
    h_split->setStretchFactor(0, 3);
    h_split->setStretchFactor(1, 1);

    root->addWidget(h_split, 1);

    // --- Status bar ---
    auto* status_row = new QHBoxLayout();
    status_row->setContentsMargins(6, 2, 6, 2);
    status_label_ = new QLabel(QStringLiteral("Ready"), this);
    status_label_->setStyleSheet(QStringLiteral("color: #aaa; font-size: 9px;"));
    progress_bar_ = new QProgressBar(this);
    progress_bar_->setMaximumWidth(200);
    progress_bar_->setMaximumHeight(14);
    progress_bar_->setRange(0, 100);
    progress_bar_->setValue(0);
    progress_bar_->hide();
    status_row->addWidget(status_label_, 1);
    status_row->addWidget(progress_bar_, 0);
    root->addLayout(status_row);

    // --- Connect signals ---
    connect(viewport_, &BioViewport3D::stepPicked,
            this, &BioSimWidget::onStepPicked);
    connect(viewport_, &BioViewport3D::colorRangeChanged,
            this, &BioSimWidget::onColorRangeChanged);
    connect(slab_editor_, &SlabEditor3D::slabsChanged,
            this, &BioSimWidget::onSlabsChanged);
}

void BioSimWidget::buildToolbar(QToolBar* tb)
{
    // CSV file selector.
    auto* btn_csv = new QPushButton(QStringLiteral("CSV…"), tb);
    btn_csv->setToolTip(QStringLiteral("Select Geant4 energy_step_profile.csv"));
    connect(btn_csv, &QPushButton::clicked, this, &BioSimWidget::onBrowseCsv);
    tb->addWidget(btn_csv);

    csv_label_ = new QLabel(QStringLiteral("(no file)"), tb);
    csv_label_->setStyleSheet(QStringLiteral("color: #aaa; font-size: 9px; margin: 0 6px;"));
    csv_label_->setMaximumWidth(200);
    tb->addWidget(csv_label_);

    tb->addSeparator();

    // Run / Stop.
    btn_run_  = new QPushButton(QStringLiteral("▶ Run"),  tb);
    btn_stop_ = new QPushButton(QStringLiteral("■ Stop"), tb);
    btn_stop_->setEnabled(false);
    connect(btn_run_,  &QPushButton::clicked, this, &BioSimWidget::onRun);
    connect(btn_stop_, &QPushButton::clicked, this, &BioSimWidget::onStop);
    tb->addWidget(btn_run_);
    tb->addWidget(btn_stop_);

    tb->addSeparator();

    // Color-by selector.
    tb->addWidget(new QLabel(QStringLiteral("Color:"), tb));
    color_by_combo_ = new QComboBox(tb);
    color_by_combo_->addItem(QStringLiteral("dE [MeV]"),    int(BioViewport3D::ColorBy::EdepMeV));
    color_by_combo_->addItem(QStringLiteral("Dose [Gy]"),   int(BioViewport3D::ColorBy::DoseGy));
    color_by_combo_->addItem(QStringLiteral("LET"),         int(BioViewport3D::ColorBy::LET));
    color_by_combo_->addItem(QStringLiteral("KinE [MeV]"),  int(BioViewport3D::ColorBy::KineticMeV));
    color_by_combo_->addItem(QStringLiteral("H [mSv]"),     int(BioViewport3D::ColorBy::SVmSv));
    connect(color_by_combo_, &QComboBox::currentIndexChanged,
            this, &BioSimWidget::onColorByChanged);
    tb->addWidget(color_by_combo_);

    // Palette selector.
    tb->addWidget(new QLabel(QStringLiteral("Palette:"), tb));
    palette_combo_ = new QComboBox(tb);
    for (int i = 0; i < EnergyColorMapper::kPaletteCount; ++i) {
        const auto p = static_cast<EnergyColorMapper::Palette>(i);
        palette_combo_->addItem(EnergyColorMapper::paletteName(p), i);
    }
    // Default to BraggPeak palette.
    palette_combo_->setCurrentIndex(int(EnergyColorMapper::Palette::BraggPeak));
    connect(palette_combo_, &QComboBox::currentIndexChanged,
            this, &BioSimWidget::onPaletteChanged);
    tb->addWidget(palette_combo_);

    tb->addSeparator();

    // Camera reset — viewport_ may not be constructed yet; capture by this.
    auto* btn_reset = new QPushButton(QStringLiteral("⟲ Camera"), tb);
    connect(btn_reset, &QPushButton::clicked, this,
            [this]() { if (viewport_) viewport_->resetCamera(); });
    tb->addWidget(btn_reset);

    // Material editor.
    auto* btn_mats = new QPushButton(QStringLiteral("Materials…"), tb);
    connect(btn_mats, &QPushButton::clicked, this, &BioSimWidget::onOpenMaterialEditor);
    tb->addWidget(btn_mats);

    // Phantom preset panel.
    auto* btn_phantom = new QPushButton(QStringLiteral("Phantoms…"), tb);
    connect(btn_phantom, &QPushButton::clicked, this, &BioSimWidget::onOpenPhantomPanel);
    tb->addWidget(btn_phantom);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void BioSimWidget::setCsvPath(const QString& path)
{
    csv_path_ = path;
    csv_label_->setText(path.section('/', -1)); // basename
    csv_label_->setToolTip(path);
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------

void BioSimWidget::onBrowseCsv()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("Select energy_step_profile.csv"),
        QString{},
        QStringLiteral("CSV files (*.csv);;All files (*)"));
    if (!path.isEmpty()) setCsvPath(path);
}

void BioSimWidget::onRun()
{
    if (csv_path_.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("No file"),
                             QStringLiteral("Please select a CSV file first."));
        return;
    }
    if (running_) return;

    running_ = true;
    btn_run_->setEnabled(false);
    btn_stop_->setEnabled(true);
    progress_bar_->setValue(0);
    progress_bar_->show();
    status_label_->setText(QStringLiteral("Running simulation…"));

    const BioSimConfig cfg = buildConfig();

    // Run async — BioSimRunner::run is thread-safe (no shared mutable state).
    QFuture<BioSimResult> future = QtConcurrent::run([cfg]() -> BioSimResult {
        BioSimRunner runner;
        return runner.run(cfg);
    });
    watcher_->setFuture(future);
}

void BioSimWidget::onStop()
{
    // We can't cancel a running future cleanly without a progress callback.
    // The simplest safe approach: ignore the result when it arrives.
    running_ = false;
    btn_run_->setEnabled(true);
    btn_stop_->setEnabled(false);
    progress_bar_->hide();
    status_label_->setText(QStringLiteral("Stopped."));
}

void BioSimWidget::onSimFinished()
{
    const bool was_running = running_;
    running_ = false;
    btn_run_->setEnabled(true);
    btn_stop_->setEnabled(false);
    progress_bar_->setValue(100);

    if (!was_running) return; // user pressed Stop

    last_result_ = watcher_->result();
    if (!last_result_.valid) {
        status_label_->setText(QStringLiteral("Error: ") +
                               QString::fromStdString(last_result_.message));
        emit simulationFinished(false, QString::fromStdString(last_result_.message));
        progress_bar_->hide();
        return;
    }

    viewport_->setResult(last_result_);
    viewport_->resetCamera();
    inspector_->setResult(last_result_);

    const QString msg = QString("Done — %1 tracks, %2 steps")
        .arg(last_result_.total_tracks)
        .arg(last_result_.total_steps);
    status_label_->setText(msg);
    progress_bar_->hide();
    emit simulationFinished(true, msg);
}

void BioSimWidget::onStepPicked(int track_idx, int step_idx)
{
    inspector_->showStep(track_idx, step_idx);
}

void BioSimWidget::onSlabsChanged(const std::vector<BioSlab>& slabs)
{
    viewport_->setSlabs(slabs);
}

void BioSimWidget::onColorByChanged(int index)
{
    const auto field = static_cast<BioViewport3D::ColorBy>(
        color_by_combo_->itemData(index).toInt());
    viewport_->setColorBy(field);
}

void BioSimWidget::onPaletteChanged(int index)
{
    const auto pal = static_cast<EnergyColorMapper::Palette>(
        palette_combo_->itemData(index).toInt());
    viewport_->setPalette(pal);
    scale_bar_->setPalette(pal);
}

void BioSimWidget::onColorRangeChanged(double min_val, double max_val,
                                        const QString& unit)
{
    scale_bar_->setRange(min_val, max_val, unit);
}

void BioSimWidget::onOpenMaterialEditor()
{
    MaterialEditorDialog dlg(this);
    dlg.exec();
}

void BioSimWidget::onOpenPhantomPanel()
{
    PhantomPresetPanel panel(this);
    connect(&panel, &PhantomPresetPanel::phantomSelected,
            this, [this](const PhantomPreset& preset) {
                slab_editor_->setSlabs(preset.slabs);
                viewport_->setSlabs(preset.slabs);
            });
    panel.exec();
}

// ---------------------------------------------------------------------------
// Config builder
// ---------------------------------------------------------------------------

BioSimConfig BioSimWidget::buildConfig() const
{
    BioSimConfig cfg;
    cfg.energy_step_csv_path = csv_path_.toStdString();
    cfg.slabs                = slab_editor_->slabs();
    return cfg;
}

} // namespace beamlab::biosim
