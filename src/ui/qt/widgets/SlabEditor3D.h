#pragma once

#include "domain/geometry/Slab.h"
#include "domain/materials/MaterialRegistry.h"
#include "ui/qt/dockable/IDockableWidget.h"

#include <QWidget>
#include <vector>

class QTableWidget;
class QPushButton;
class QLabel;

namespace beamlab::ui {

class SlabEditor3D : public QWidget, public qt::IDockableWidget {
    Q_OBJECT

public:
    explicit SlabEditor3D(
        domain::materials::MaterialRegistry* materialRegistry,
        QWidget* parent = nullptr);

    // ── IDockableWidget ───────────────────────────────────────────
    QString title() const override { return QStringLiteral("Slab Editor"); }
    QString id() const override { return QStringLiteral("slab_editor"); }
    QWidget* widget() override { return this; }
    Qt::DockWidgetArea preferredArea() const override
        { return Qt::RightDockWidgetArea; }

    void setSlabs(const std::vector<domain::geometry::Slab>& slabs);
    [[nodiscard]] const std::vector<domain::geometry::Slab>& slabs() const
    {
        return slabs_;
    }

signals:
    void slabsChanged(const std::vector<domain::geometry::Slab>& slabs);

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
    void installMaterialCombo(int row);

    std::vector<domain::geometry::Slab> slabs_;
    domain::materials::MaterialRegistry* materialRegistry_;

    bool updating_table_{false};

    QTableWidget* table_{nullptr};
    QPushButton*  btn_add_{nullptr};
    QPushButton*  btn_dup_{nullptr};
    QPushButton*  btn_rem_{nullptr};
    QPushButton*  btn_up_{nullptr};
    QPushButton*  btn_dn_{nullptr};

    static constexpr int kColIdx      = 0;
    static constexpr int kColLabel    = 1;
    static constexpr int kColMaterial = 2;
    static constexpr int kColStart    = 3;
    static constexpr int kColThick    = 4;
    static constexpr int kColShape    = 5;
    static constexpr int kColEnabled  = 6;
    static constexpr int kNumCols     = 7;
};

} // namespace beamlab::ui
