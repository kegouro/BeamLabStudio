#include "biosim/ui/qt/SlabEditor3D.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <algorithm>

namespace beamlab::biosim {

static QString shapeLabel(BioSlab::Shape s) {
    switch (s) {
    case BioSlab::Shape::Infinite:  return QStringLiteral("∞ Inf");
    case BioSlab::Shape::Rectangle: return QStringLiteral("Rect");
    case BioSlab::Shape::Cylinder:  return QStringLiteral("Cyl");
    case BioSlab::Shape::Ellipse:   return QStringLiteral("Ell");
    }
    return {};
}

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

SlabEditor3D::SlabEditor3D(QWidget* parent)
    : QWidget(parent)
{
    buildLayout();
}

void SlabEditor3D::buildLayout()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(4, 4, 4, 4);
    root->setSpacing(3);

    // --- Toolbar ---
    auto* toolbar = new QHBoxLayout();
    auto mkBtn = [&](const QString& text, const QString& tip) {
        auto* b = new QPushButton(text, this);
        b->setToolTip(tip);
        b->setMaximumWidth(80);
        toolbar->addWidget(b);
        return b;
    };
    btn_add_ = mkBtn(QStringLiteral("+ Add"),       QStringLiteral("Add a new slab"));
    btn_dup_ = mkBtn(QStringLiteral("Duplicate"),   QStringLiteral("Duplicate selected slab"));
    btn_rem_ = mkBtn(QStringLiteral("✕ Remove"),    QStringLiteral("Remove selected slab"));
    btn_up_  = mkBtn(QStringLiteral("▲"),           QStringLiteral("Move slab up"));
    btn_dn_  = mkBtn(QStringLiteral("▼"),           QStringLiteral("Move slab down"));
    toolbar->addStretch();
    root->addLayout(toolbar);

    // --- Table ---
    table_ = new QTableWidget(0, kNumCols, this);
    table_->setHorizontalHeaderLabels({
        "#", "Label", "Material", "Start [m]", "Thick [m]", "Shape", "En"
    });
    table_->horizontalHeader()->setSectionResizeMode(kColLabel,    QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(kColMaterial, QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(kColIdx,      QHeaderView::Fixed);
    table_->horizontalHeader()->setSectionResizeMode(kColEnabled,  QHeaderView::Fixed);
    table_->setColumnWidth(kColIdx,     28);
    table_->setColumnWidth(kColEnabled, 28);
    table_->verticalHeader()->setDefaultSectionSize(22);
    table_->setAlternatingRowColors(true);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setStyleSheet(QStringLiteral(
        "QTableWidget { font-size: 9px; background: #0d0d1a; color: #ddd; }"
        "QHeaderView::section { background: #1a1a2e; color: #88aaff; font-size: 9px; }"));
    root->addWidget(table_, 1);

    // --- Connections ---
    connect(btn_add_, &QPushButton::clicked, this, &SlabEditor3D::onAddSlab);
    connect(btn_dup_, &QPushButton::clicked, this, &SlabEditor3D::onDuplicateSlab);
    connect(btn_rem_, &QPushButton::clicked, this, &SlabEditor3D::onRemoveSlab);
    connect(btn_up_,  &QPushButton::clicked, this, &SlabEditor3D::onMoveUp);
    connect(btn_dn_,  &QPushButton::clicked, this, &SlabEditor3D::onMoveDown);
    connect(table_,   &QTableWidget::cellChanged,
            this, &SlabEditor3D::onCellChanged);
    connect(table_->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &SlabEditor3D::onSelectionChanged);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void SlabEditor3D::setSlabs(const std::vector<BioSlab>& slabs)
{
    slabs_ = slabs;
    refreshTable();
}

// ---------------------------------------------------------------------------
// Table management
// ---------------------------------------------------------------------------

void SlabEditor3D::refreshTable()
{
    updating_table_ = true;
    table_->setRowCount(static_cast<int>(slabs_.size()));
    for (int r = 0; r < static_cast<int>(slabs_.size()); ++r)
        refreshRow(r);
    updating_table_ = false;
}

void SlabEditor3D::refreshRow(int row)
{
    const auto& s = slabs_[static_cast<std::size_t>(row)];

    // Index label (read-only).
    auto* idx_item = new QTableWidgetItem(QString::number(row));
    idx_item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    idx_item->setTextAlignment(Qt::AlignCenter);
    table_->setItem(row, kColIdx, idx_item);

    // Label (editable text).
    auto* lbl = new QTableWidgetItem(QString::fromStdString(s.label));
    table_->setItem(row, kColLabel, lbl);

    // Material combo (embedded widget).
    installMaterialCombo(row);

    // Start position spin.
    auto* start_spin = new QDoubleSpinBox();
    start_spin->setRange(-10.0, 10.0);
    start_spin->setDecimals(4);
    start_spin->setSingleStep(0.001);
    start_spin->setValue(s.axial_start_m);
    start_spin->setSuffix(QStringLiteral(" m"));
    connect(start_spin, &QDoubleSpinBox::valueChanged, this,
            [this, row](double v) {
                if (updating_table_) return;
                slabs_[static_cast<std::size_t>(row)].axial_start_m = v;
                emitChanged();
            });
    table_->setCellWidget(row, kColStart, start_spin);

    // Thickness spin.
    auto* thick_spin = new QDoubleSpinBox();
    thick_spin->setRange(0.0001, 5.0);
    thick_spin->setDecimals(4);
    thick_spin->setSingleStep(0.001);
    thick_spin->setValue(s.thickness_m);
    thick_spin->setSuffix(QStringLiteral(" m"));
    connect(thick_spin, &QDoubleSpinBox::valueChanged, this,
            [this, row](double v) {
                if (updating_table_) return;
                slabs_[static_cast<std::size_t>(row)].thickness_m = v;
                emitChanged();
            });
    table_->setCellWidget(row, kColThick, thick_spin);

    // Shape combo.
    auto* shape_combo = new QComboBox();
    shape_combo->addItem(shapeLabel(BioSlab::Shape::Infinite),
                         int(BioSlab::Shape::Infinite));
    shape_combo->addItem(shapeLabel(BioSlab::Shape::Rectangle),
                         int(BioSlab::Shape::Rectangle));
    shape_combo->addItem(shapeLabel(BioSlab::Shape::Cylinder),
                         int(BioSlab::Shape::Cylinder));
    shape_combo->addItem(shapeLabel(BioSlab::Shape::Ellipse),
                         int(BioSlab::Shape::Ellipse));
    shape_combo->setCurrentIndex(static_cast<int>(s.shape));
    connect(shape_combo, &QComboBox::currentIndexChanged, this,
            [this, row](int idx) {
                if (updating_table_) return;
                slabs_[static_cast<std::size_t>(row)].shape =
                    static_cast<BioSlab::Shape>(idx);
                emitChanged();
            });
    table_->setCellWidget(row, kColShape, shape_combo);

    // Enabled checkbox.
    auto* chk = new QCheckBox();
    chk->setChecked(s.enabled);
    chk->setStyleSheet(QStringLiteral("margin-left: 5px;"));
    connect(chk, &QCheckBox::toggled, this,
            [this, row](bool on) {
                if (updating_table_) return;
                slabs_[static_cast<std::size_t>(row)].enabled = on;
                emitChanged();
            });
    table_->setCellWidget(row, kColEnabled, chk);
}

void SlabEditor3D::installMaterialCombo(int row)
{
    auto* combo = new QComboBox();
    const auto& all_mats = mat_lib_.all();
    int current_idx = 0;
    const std::string& cur_id = slabs_[static_cast<std::size_t>(row)].material.id;

    for (int mi = 0; mi < static_cast<int>(all_mats.size()); ++mi) {
        const auto& mat = all_mats[static_cast<std::size_t>(mi)];
        combo->addItem(QString::fromStdString(mat.name + "  [" + mat.category + "]"),
                       QString::fromStdString(mat.id));
        if (mat.id == cur_id) current_idx = mi;
    }
    combo->setCurrentIndex(current_idx);

    connect(combo, &QComboBox::currentIndexChanged, this,
            [this, row, combo](int) {
                if (updating_table_) return;
                const std::string id = combo->currentData().toString().toStdString();
                const auto mat = mat_lib_.findById(id);
                if (mat) {
                    slabs_[static_cast<std::size_t>(row)].material = *mat;
                    emitChanged();
                }
            });
    table_->setCellWidget(row, kColMaterial, combo);
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------

void SlabEditor3D::onAddSlab()
{
    BioSlab slab;
    slab.id    = "slab_" + std::to_string(slabs_.size());
    slab.label = "Slab " + std::to_string(slabs_.size() + 1);

    // Default to water.
    const auto water = mat_lib_.findById("water");
    if (water) slab.material = *water;

    // Place just after the last slab.
    if (!slabs_.empty()) {
        const auto& last = slabs_.back();
        slab.axial_start_m = last.axial_start_m + last.thickness_m + 0.005;
    }

    slabs_.push_back(std::move(slab));
    refreshTable();
    table_->selectRow(static_cast<int>(slabs_.size()) - 1);
    emitChanged();
}

void SlabEditor3D::onDuplicateSlab()
{
    const auto rows = table_->selectionModel()->selectedRows();
    if (rows.isEmpty()) return;
    const int row = rows.first().row();
    BioSlab copy = slabs_[static_cast<std::size_t>(row)];
    copy.id    += "_copy";
    copy.label += " (copy)";
    copy.axial_start_m += copy.thickness_m + 0.002;
    slabs_.insert(slabs_.begin() + row + 1, copy);
    refreshTable();
    table_->selectRow(row + 1);
    emitChanged();
}

void SlabEditor3D::onRemoveSlab()
{
    const auto rows = table_->selectionModel()->selectedRows();
    if (rows.isEmpty()) return;
    const int row = rows.first().row();
    slabs_.erase(slabs_.begin() + row);
    refreshTable();
    if (!slabs_.empty())
        table_->selectRow(std::min(row, static_cast<int>(slabs_.size()) - 1));
    emitChanged();
}

void SlabEditor3D::onMoveUp()
{
    const auto rows = table_->selectionModel()->selectedRows();
    if (rows.isEmpty()) return;
    int row = rows.first().row();
    if (row <= 0) return;
    std::swap(slabs_[static_cast<std::size_t>(row)],
              slabs_[static_cast<std::size_t>(row - 1)]);
    refreshTable();
    table_->selectRow(row - 1);
    emitChanged();
}

void SlabEditor3D::onMoveDown()
{
    const auto rows = table_->selectionModel()->selectedRows();
    if (rows.isEmpty()) return;
    int row = rows.first().row();
    if (row >= static_cast<int>(slabs_.size()) - 1) return;
    std::swap(slabs_[static_cast<std::size_t>(row)],
              slabs_[static_cast<std::size_t>(row + 1)]);
    refreshTable();
    table_->selectRow(row + 1);
    emitChanged();
}

void SlabEditor3D::onCellChanged(int row, int col)
{
    if (updating_table_) return;
    if (col == kColLabel) {
        const auto* item = table_->item(row, kColLabel);
        if (item)
            slabs_[static_cast<std::size_t>(row)].label = item->text().toStdString();
        emitChanged();
    }
}

void SlabEditor3D::onSelectionChanged()
{
    const auto rows = table_->selectionModel()->selectedRows();
    const bool has = !rows.isEmpty();
    const bool first = has && (rows.first().row() == 0);
    const bool last  = has && (rows.first().row() == static_cast<int>(slabs_.size()) - 1);
    btn_dup_->setEnabled(has);
    btn_rem_->setEnabled(has);
    btn_up_->setEnabled(has && !first);
    btn_dn_->setEnabled(has && !last);
}

void SlabEditor3D::emitChanged()
{
    emit slabsChanged(slabs_);
}

} // namespace beamlab::biosim
