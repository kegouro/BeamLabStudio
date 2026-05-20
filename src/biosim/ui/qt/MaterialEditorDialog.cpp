#include "biosim/ui/qt/MaterialEditorDialog.h"
#include "biosim/physics/ParticleLibrary.h"

#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPainter>
#include <QPaintEvent>
#include <QPushButton>
#include <QSplitter>
#include <QTabWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace beamlab::biosim {

// ---------------------------------------------------------------------------
// Inline dE/dx preview widget
// ---------------------------------------------------------------------------

class DedxPlotWidget : public QWidget {
public:
    explicit DedxPlotWidget(QWidget* parent = nullptr) : QWidget(parent)
    {
        setMinimumHeight(110);
        setMaximumHeight(160);
    }

    void setCurve(const std::vector<std::pair<double,double>>& pts,
                  const QString& title)
    {
        pts_   = pts;
        title_ = title;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.fillRect(rect(), QColor(14, 14, 24));

        if (pts_.empty()) {
            p.setPen(QColor(100, 100, 100));
            p.drawText(rect(), Qt::AlignCenter, QStringLiteral("No data"));
            return;
        }

        const int W = width()  - 8;
        const int H = height() - 22;
        const int ox = 4, oy = 4;

        double xmin = std::log10(pts_.front().first);
        double xmax = std::log10(pts_.back().first);
        double ymin = std::numeric_limits<double>::max();
        double ymax = -ymin;
        for (const auto& [x, y] : pts_) { ymin = std::min(ymin, y); ymax = std::max(ymax, y); }

        if (xmax <= xmin || ymax <= ymin) return;

        auto toX = [&](double logE) { return ox + (logE - xmin) / (xmax - xmin) * W; };
        auto toY = [&](double v)    { return oy + H - (v - ymin) / (ymax - ymin) * H; };

        // Grid.
        p.setPen(QColor(40, 40, 60));
        for (int i = 0; i <= 4; ++i) {
            const int y = oy + i * H / 4;
            p.drawLine(ox, y, ox + W, y);
        }

        // Curve.
        p.setPen(QPen(QColor(80, 200, 255), 1.8));
        for (std::size_t i = 1; i < pts_.size(); ++i) {
            p.drawLine(
                QPointF(toX(std::log10(pts_[i-1].first)), toY(pts_[i-1].second)),
                QPointF(toX(std::log10(pts_[i].first)),   toY(pts_[i].second)));
        }

        // Axes labels.
        QFont f; f.setPointSizeF(7.0); p.setFont(f);
        p.setPen(QColor(160, 160, 160));
        p.drawText(QRect(ox, oy + H + 2, W, 14), Qt::AlignLeft,
                   QString::number(std::pow(10.0, xmin), 'g', 2) + " MeV");
        p.drawText(QRect(ox, oy + H + 2, W, 14), Qt::AlignRight,
                   QString::number(std::pow(10.0, xmax), 'g', 2) + " MeV");
        p.drawText(QRect(ox, oy, W, 14), Qt::AlignRight,
                   QString::number(ymax, 'g', 3) + " MeV/cm");

        // Title.
        p.setPen(QColor(220, 220, 220));
        QFont tf; tf.setPointSizeF(7.5); tf.setBold(true); p.setFont(tf);
        p.drawText(QRect(ox, oy + H + 10, W, 12), Qt::AlignCenter, title_);
    }

private:
    std::vector<std::pair<double, double>> pts_{};
    QString title_{};
};

// ---------------------------------------------------------------------------
// MaterialEditorDialog
// ---------------------------------------------------------------------------

MaterialEditorDialog::MaterialEditorDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Material Gallery & Editor"));
    setMinimumSize(820, 560);
    buildLayout();
    populateList();
}

std::optional<BioMaterial> MaterialEditorDialog::selectedMaterial() const
{
    return selected_;
}

void MaterialEditorDialog::buildLayout()
{
    auto* root = new QHBoxLayout(this);

    // ── Left panel (list + search + buttons) ─────────────────────────────
    auto* left = new QVBoxLayout();

    auto* search_row = new QHBoxLayout();
    search_edit_ = new QLineEdit(this);
    search_edit_->setPlaceholderText(QStringLiteral("Search materials…"));
    connect(search_edit_, &QLineEdit::textChanged,
            this, &MaterialEditorDialog::onSearchChanged);
    search_row->addWidget(new QLabel(QStringLiteral("🔍"), this));
    search_row->addWidget(search_edit_);
    left->addLayout(search_row);

    list_widget_ = new QListWidget(this);
    list_widget_->setAlternatingRowColors(true);
    list_widget_->setStyleSheet(QStringLiteral(
        "QListWidget { font-size: 9px; background: #0d0d1a; color: #ddd; }"
        "QListWidget::item:selected { background: #1a3a6e; }"));
    connect(list_widget_, &QListWidget::currentRowChanged,
            this, &MaterialEditorDialog::onMaterialSelected);
    left->addWidget(list_widget_, 1);

    auto* btn_row = new QHBoxLayout();
    btn_add_ = new QPushButton(QStringLiteral("+ New"), this);
    btn_del_ = new QPushButton(QStringLiteral("✕ Delete"), this);
    btn_del_->setEnabled(false);
    connect(btn_add_, &QPushButton::clicked, this, &MaterialEditorDialog::onAddCustom);
    connect(btn_del_, &QPushButton::clicked, this, &MaterialEditorDialog::onDeleteCustom);
    btn_row->addWidget(btn_add_);
    btn_row->addWidget(btn_del_);
    left->addLayout(btn_row);

    // ── Right panel (tabs + preview) ─────────────────────────────────────
    auto* right = new QVBoxLayout();

    auto* tabs = new QTabWidget(this);

    // Physical params tab.
    auto* phys_widget = new QWidget(tabs);
    auto* phys_form   = new QFormLayout(phys_widget);
    phys_form->setRowWrapPolicy(QFormLayout::WrapAllRows);
    auto mkSpin = [&](double min, double max, double step, int dec) {
        auto* s = new QDoubleSpinBox(this);
        s->setRange(min, max); s->setSingleStep(step); s->setDecimals(dec);
        return s;
    };
    edit_id_       = new QLineEdit(this);
    edit_name_     = new QLineEdit(this);
    edit_category_ = new QLineEdit(this);
    spin_density_  = mkSpin(1e-6, 25.0, 0.01, 4);
    spin_Z_eff_    = mkSpin(1.0, 100.0, 0.1, 2);
    spin_A_eff_    = mkSpin(1.0, 250.0, 0.1, 2);
    spin_I_eV_     = mkSpin(10.0, 1000.0, 1.0, 1);
    phys_form->addRow(QStringLiteral("ID:"),           edit_id_);
    phys_form->addRow(QStringLiteral("Name:"),         edit_name_);
    phys_form->addRow(QStringLiteral("Category:"),     edit_category_);
    phys_form->addRow(QStringLiteral("ρ [g/cm³]:"),    spin_density_);
    phys_form->addRow(QStringLiteral("Z_eff:"),        spin_Z_eff_);
    phys_form->addRow(QStringLiteral("A_eff [g/mol]:"),spin_A_eff_);
    phys_form->addRow(QStringLiteral("I [eV]:"),       spin_I_eV_);
    tabs->addTab(phys_widget, QStringLiteral("Physical"));

    // Connect all physical controls to live-update the dE/dx preview.
    for (QDoubleSpinBox* sp : {spin_density_, spin_Z_eff_, spin_A_eff_, spin_I_eV_})
        connect(sp, &QDoubleSpinBox::valueChanged, this,
                &MaterialEditorDialog::updateDedxPlot);

    // Biological params tab.
    auto* bio_widget = new QWidget(tabs);
    auto* bio_form   = new QFormLayout(bio_widget);
    spin_WR_         = mkSpin(1.0, 20.0, 0.1, 1);
    spin_WT_         = mkSpin(0.0,  1.0, 0.01, 3);
    spin_alpha_beta_ = mkSpin(0.1, 100.0, 0.5, 1);
    edit_organ_      = new QLineEdit(this);
    bio_form->addRow(QStringLiteral("WR (ICRP-103):"),       spin_WR_);
    bio_form->addRow(QStringLiteral("WT (ICRP-103):"),       spin_WT_);
    bio_form->addRow(QStringLiteral("α/β [Gy] (LQ model):"), spin_alpha_beta_);
    bio_form->addRow(QStringLiteral("Organ name:"),           edit_organ_);

    auto* bio_help = new QLabel(
        QStringLiteral("<small>WR: radiation weighting factor (photon=1, proton=2, α=20).<br>"
                       "WT: tissue weighting factor (gonads=0.08, lung=0.12, …).<br>"
                       "α/β ratio: typical ~10 Gy for early-responding tissue.</small>"),
        bio_widget);
    bio_help->setWordWrap(true);
    bio_help->setStyleSheet(QStringLiteral("color: #888;"));
    bio_form->addRow(bio_help);
    tabs->addTab(bio_widget, QStringLiteral("Biological"));

    right->addWidget(tabs, 2);

    // dE/dx preview.
    auto* preview_group = new QGroupBox(QStringLiteral("dE/dx preview (proton, Bethe-Bloch)"), this);
    auto* preview_layout = new QVBoxLayout(preview_group);
    dedx_plot_ = new DedxPlotWidget(this);
    preview_layout->addWidget(dedx_plot_);
    right->addWidget(preview_group, 1);

    // Save button.
    btn_save_ = new QPushButton(QStringLiteral("Save changes"), this);
    btn_save_->setEnabled(false);
    connect(btn_save_, &QPushButton::clicked, this, &MaterialEditorDialog::onSaveEdits);
    right->addWidget(btn_save_);

    // Close button.
    auto* close_box = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(close_box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    right->addWidget(close_box);

    // Assemble.
    auto* left_widget = new QWidget(this);
    left_widget->setLayout(left);
    left_widget->setMaximumWidth(280);

    auto* right_widget = new QWidget(this);
    right_widget->setLayout(right);

    root->addWidget(left_widget);
    root->addWidget(right_widget, 1);
}

// ---------------------------------------------------------------------------
// List population
// ---------------------------------------------------------------------------

void MaterialEditorDialog::populateList(const QString& filter)
{
    list_widget_->clear();
    const auto& all = library_.all();
    QString last_cat;
    for (const auto& mat : all) {
        const QString name = QString::fromStdString(mat.name);
        const QString cat  = QString::fromStdString(mat.category);
        if (!filter.isEmpty() &&
            !name.contains(filter, Qt::CaseInsensitive) &&
            !cat.contains(filter, Qt::CaseInsensitive)) continue;

        // Category separator.
        if (cat != last_cat) {
            auto* sep = new QListWidgetItem(QStringLiteral("── ") + cat +
                                            QStringLiteral(" ──"));
            sep->setFlags(Qt::NoItemFlags);
            sep->setForeground(QColor(100, 160, 255));
            QFont f; f.setBold(true); f.setPointSizeF(8.0); sep->setFont(f);
            list_widget_->addItem(sep);
            last_cat = cat;
        }

        auto* item = new QListWidgetItem(QStringLiteral("  ") + name);
        item->setData(Qt::UserRole, QString::fromStdString(mat.id));
        item->setToolTip(QString("ρ=%1 g/cm³  Z=%2  I=%3 eV")
                         .arg(mat.density_g_cm3, 0, 'f', 3)
                         .arg(mat.Z_eff, 0, 'f', 1)
                         .arg(mat.I_eV, 0, 'f', 1));
        list_widget_->addItem(item);
    }
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------

void MaterialEditorDialog::onMaterialSelected(int row)
{
    const auto* item = list_widget_->item(row);
    if (!item || !(item->flags() & Qt::ItemIsEnabled)) return;
    const std::string id = item->data(Qt::UserRole).toString().toStdString();
    const auto mat = library_.findById(id);
    if (!mat) return;

    editing_ = *mat;
    selected_ = *mat;
    loadMaterialToForm(editing_);
    btn_save_->setEnabled(editing_.is_custom);
    btn_del_->setEnabled(editing_.is_custom);
    updateDedxPlot();
}

void MaterialEditorDialog::onAddCustom()
{
    BioMaterial mat;
    mat.id        = "custom_" + std::to_string(library_.all().size());
    mat.name      = "Custom material";
    mat.category  = "Custom";
    mat.density_g_cm3 = 1.0;
    mat.Z_eff     = 7.0;
    mat.A_eff     = 14.0;
    mat.I_eV      = 75.0;
    mat.WR        = 1.0;
    mat.is_custom = true;

    library_.addCustom(mat);
    populateList(search_edit_->text());
    btn_save_->setEnabled(true);
    editing_ = mat;
    loadMaterialToForm(mat);
    emit libraryChanged();
}

void MaterialEditorDialog::onDeleteCustom()
{
    if (!editing_.is_custom) return;
    const auto reply = QMessageBox::question(
        this, QStringLiteral("Delete"),
        QString("Delete custom material \"%1\"?")
            .arg(QString::fromStdString(editing_.name)));
    if (reply != QMessageBox::Yes) return;

    library_.removeCustom(editing_.id);
    populateList(search_edit_->text());
    selected_.reset();
    emit libraryChanged();
}

void MaterialEditorDialog::onSaveEdits()
{
    editing_ = readMaterialFromForm();
    library_.addCustom(editing_); // addCustom replaces if same id
    selected_ = editing_;
    populateList(search_edit_->text());
    emit libraryChanged();
}

void MaterialEditorDialog::onSearchChanged(const QString& text)
{
    populateList(text);
}

// ---------------------------------------------------------------------------
// Form helpers
// ---------------------------------------------------------------------------

void MaterialEditorDialog::loadMaterialToForm(const BioMaterial& mat)
{
    edit_id_->setText(QString::fromStdString(mat.id));
    edit_name_->setText(QString::fromStdString(mat.name));
    edit_category_->setText(QString::fromStdString(mat.category));
    spin_density_->setValue(mat.density_g_cm3);
    spin_Z_eff_->setValue(mat.Z_eff);
    spin_A_eff_->setValue(mat.A_eff);
    spin_I_eV_->setValue(mat.I_eV);
    spin_WR_->setValue(mat.WR);
    spin_WT_->setValue(mat.WT);
    spin_alpha_beta_->setValue(mat.alpha_beta_ratio);
    edit_organ_->setText(QString::fromStdString(mat.organ_name));
}

BioMaterial MaterialEditorDialog::readMaterialFromForm() const
{
    BioMaterial mat = editing_;
    mat.id              = edit_id_->text().toStdString();
    mat.name            = edit_name_->text().toStdString();
    mat.category        = edit_category_->text().toStdString();
    mat.density_g_cm3   = spin_density_->value();
    mat.Z_eff           = spin_Z_eff_->value();
    mat.A_eff           = spin_A_eff_->value();
    mat.I_eV            = spin_I_eV_->value();
    mat.WR               = spin_WR_->value();
    mat.WT               = spin_WT_->value();
    mat.alpha_beta_ratio = spin_alpha_beta_->value();
    mat.organ_name       = edit_organ_->text().toStdString();
    mat.is_custom       = true;
    return mat;
}

void MaterialEditorDialog::updateDedxPlot()
{
    auto* plot = static_cast<DedxPlotWidget*>(dedx_plot_);
    if (!plot) return;

    BioMaterial mat = readMaterialFromForm();
    if (mat.density_g_cm3 <= 0 || mat.Z_eff <= 0 || mat.I_eV <= 0) {
        plot->setCurve({}, {});
        return;
    }

    // Sample 60 log-spaced kinetic energies for a proton.
    constexpr int kN = 60;
    const ParticleSpecies proton = ParticleLibrary::proton();
    std::vector<std::pair<double,double>> pts;
    pts.reserve(kN);
    for (int i = 0; i < kN; ++i) {
        const double logE = -1.0 + i * (4.0 / (kN - 1)); // 0.1 MeV → 10 GeV
        const double E = std::pow(10.0, logE);
        const double dedx = spe_.dEdx_MeV_cm(E, mat, proton);
        if (dedx > 0.0) pts.push_back({E, dedx});
    }

    const QString title = QString::fromStdString(mat.name) +
                          QStringLiteral(" — proton stopping power");
    plot->setCurve(pts, title);
}

} // namespace beamlab::biosim
