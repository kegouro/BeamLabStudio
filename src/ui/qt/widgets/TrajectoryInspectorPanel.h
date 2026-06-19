#pragma once

#include "biosim/core/BioSimResult.h"
#include "biosim/ui/qt/EnergyColorMapper.h"
#include "ui/qt/dockable/IDockableWidget.h"

#include <QWidget>

#include <functional>
#include <vector>

// Forward declarations.
class QLabel;
class QTableWidget;
class QComboBox;
class QGroupBox;
class QSplitter;

namespace beamlab::biosim {

// Right-side panel shown when the user clicks a trajectory step in BioViewport3D.
//
// Layout (top-to-bottom):
//   ┌─────────────────────────────────────┐
//   │  Track header: ID, particle, energy │  ← QLabel
//   ├─────────────────────────────────────┤
//   │  Energy scale selector combo        │  ← QComboBox
//   ├─────────────────────────────────────┤
//   │  Step table: step | depth | edep …  │  ← QTableWidget (scrollable)
//   ├─────────────────────────────────────┤
//   │  Energy profile mini-plot (QPainter)│  ← EnergyProfilePlot (inner class)
//   ├─────────────────────────────────────┤
//   │  Per-slab summary table             │  ← QTableWidget
//   └─────────────────────────────────────┘
class TrajectoryInspectorPanel : public QWidget,
                                  public beamlab::ui::qt::IDockableWidget {
    Q_OBJECT

public:
    explicit TrajectoryInspectorPanel(QWidget* parent = nullptr);

    // ── IDockableWidget ───────────────────────────────────────────
    QString title() const override
        { return QStringLiteral("Inspector"); }
    QString id() const override
        { return QStringLiteral("inspector"); }
    QWidget* widget() override { return this; }
    Qt::DockWidgetArea preferredArea() const override
        { return Qt::RightDockWidgetArea; }

    // Load a full simulation result (needed for per-slab summary).
    void setResult(const BioSimResult& result);

    // Highlight a specific track/step (called from BioViewport3D::stepPicked).
    void showStep(int track_idx, int step_idx);

    // Clear the panel back to empty state.
    void clear();

signals:
    // Emitted when the user changes the scale group (main window may sync scale bar).
    void scaleGroupChanged(ScaleGroup group);

private:
    void buildLayout();
    void populateHeader(int track_idx) const;
    void populateStepTable(int track_idx) const;
    void populateSlabSummary() const;
    void highlightRow(int step_idx) const;
    void populateEnergyPlot(int track_idx) const;

    // Which columns to show in the step table for the current ScaleGroup.
    struct ColumnDef { QString header; std::function<double(const EnergyScaleSet&)> getter; };
    std::vector<ColumnDef> columnsForGroup(ScaleGroup g) const;

    BioSimResult result_{};
    bool has_result_{false};
    int current_track_{-1};
    int current_step_{-1};
    ScaleGroup scale_group_{ScaleGroup::Physical};

    // Widgets (owned by layout).
    QLabel*        header_label_{nullptr};
    QComboBox*     scale_combo_{nullptr};
    QTableWidget*  step_table_{nullptr};
    QWidget*       energy_plot_{nullptr};  // custom QPainter widget
    QTableWidget*  slab_table_{nullptr};

    EnergyColorMapper mapper_;
    EnergyColorMapper::Palette palette_{EnergyColorMapper::Palette::BraggPeak};
};

} // namespace beamlab::biosim
