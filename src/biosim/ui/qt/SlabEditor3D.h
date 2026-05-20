#pragma once

#include "biosim/geometry/BioSlab.h"
#include "biosim/materials/BioMaterialLibrary.h"

#include <QWidget>
#include <vector>

// Forward declarations.
class QTableWidget;
class QPushButton;
class QLabel;

namespace beamlab::biosim {

// Bottom panel: manages the list of BioSlabs that the user places in the scene.
//
// Layout:
//   [ Add ] [ Duplicate ] [ Remove ] [ ↑ ] [ ↓ ]   ← toolbar
//   ┌─────────────────────────────────────────────┐
//   │ # │ Label │ Material │ Start[m] │ Thick[m] │ Shape │ En │
//   └─────────────────────────────────────────────┘
//
// Double-clicking a row opens a small inline edit form below the table
// for fine-grained parameter editing (label, material combo, geometry).
class SlabEditor3D : public QWidget {
    Q_OBJECT

public:
    explicit SlabEditor3D(QWidget* parent = nullptr);

    // Replace the full slab list (called when loading a phantom preset).
    void setSlabs(const std::vector<BioSlab>& slabs);

    // Current slab list (read by BioSimWidget to build BioSimConfig).
    [[nodiscard]] const std::vector<BioSlab>& slabs() const { return slabs_; }

signals:
    // Emitted any time the slab list or any slab parameter changes.
    void slabsChanged(const std::vector<BioSlab>& slabs);

private slots:
    void onAddSlab();
    void onDuplicateSlab();
    void onRemoveSlab();
    void onMoveUp();
    void onMoveDown();
    void onCellChanged(int row, int col);
    void onSelectionChanged();

private:
    void buildLayout();
    void refreshTable();
    void refreshRow(int row);
    void emitChanged();

    // Populate the material combo for a given row (called lazily).
    void installMaterialCombo(int row);

    std::vector<BioSlab> slabs_{};
    BioMaterialLibrary mat_lib_{};

    bool updating_table_{false}; // guard against recursive cellChanged

    QTableWidget* table_{nullptr};
    QPushButton*  btn_add_{nullptr};
    QPushButton*  btn_dup_{nullptr};
    QPushButton*  btn_rem_{nullptr};
    QPushButton*  btn_up_{nullptr};
    QPushButton*  btn_dn_{nullptr};

    // Column indices (kept as constants for readability).
    static constexpr int kColIdx      = 0;
    static constexpr int kColLabel    = 1;
    static constexpr int kColMaterial = 2;
    static constexpr int kColStart    = 3;
    static constexpr int kColThick    = 4;
    static constexpr int kColShape    = 5;
    static constexpr int kColEnabled  = 6;
    static constexpr int kNumCols     = 7;
};

} // namespace beamlab::biosim
