#pragma once

#include "biosim/core/BioSimConfig.h"
#include "biosim/core/BioSimResult.h"
#include "biosim/ui/qt/BioViewport3D.h"
#include "biosim/ui/qt/EnergyColorMapper.h"
#include "biosim/ui/qt/EnergyScaleBar.h"
#include "biosim/ui/qt/SlabEditor3D.h"
#include "biosim/ui/qt/TrajectoryInspectorPanel.h"

#include <QFutureWatcher>
#include <QWidget>

// Forward declarations.
class QAction;
class QComboBox;
class QLabel;
class QProgressBar;
class QPushButton;
class QSplitter;
class QToolBar;

namespace beamlab::biosim {

// Main tab widget for the biological simulator.
//
// Layout:
//   ┌─────────────────────────────────────────────────────────────────┐
//   │  [Toolbar: CSV… | Run | Stop | ColorBy▾ | Palette▾ | Reset cam] │
//   ├────────────────────────────────┬──────────────────┬─────────────┤
//   │                                │                  │             │
//   │        BioViewport3D           │  EnergyScaleBar  │  Inspector  │
//   │     (3D trajectory scene)      │  (colour legend) │   Panel     │
//   │                                │                  │             │
//   ├────────────────────────────────┴──────────────────┴─────────────┤
//   │              SlabEditor3D  (bottom slab table)                  │
//   ├─────────────────────────────────────────────────────────────────┤
//   │  [Status label]                              [QProgressBar]     │
//   └─────────────────────────────────────────────────────────────────┘
class BioSimWidget : public QWidget {
    Q_OBJECT

public:
    explicit BioSimWidget(QWidget* parent = nullptr);

    // Optional: pre-load a CSV file path on startup.
    void setCsvPath(const QString& path);

signals:
    // Forwarded from BioViewport3D — can be used by MainWindow to show status.
    void simulationFinished(bool success, const QString& message);

private slots:
    void onBrowseCsv();
    void onRun();
    void onStop();
    void onSimFinished();
    void onStepPicked(int track_idx, int step_idx);
    void onSlabsChanged(const std::vector<BioSlab>& slabs);
    void onColorByChanged(int index);
    void onPaletteChanged(int index);
    void onColorRangeChanged(double min_val, double max_val, const QString& unit);
    void onOpenMaterialEditor();
    void onOpenPhantomPanel();

private:
    void buildLayout();
    void buildToolbar(QToolBar* tb);
    BioSimConfig buildConfig() const;

    // Subwidgets.
    BioViewport3D*           viewport_{nullptr};
    EnergyScaleBar*          scale_bar_{nullptr};
    TrajectoryInspectorPanel* inspector_{nullptr};
    SlabEditor3D*            slab_editor_{nullptr};

    // Toolbar controls.
    QLabel*       csv_label_{nullptr};
    QComboBox*    color_by_combo_{nullptr};
    QComboBox*    palette_combo_{nullptr};
    QPushButton*  btn_run_{nullptr};
    QPushButton*  btn_stop_{nullptr};

    // Status bar.
    QLabel*       status_label_{nullptr};
    QProgressBar* progress_bar_{nullptr};

    // State.
    QString       csv_path_{};
    BioSimResult  last_result_{};
    bool          running_{false};

    // Async simulation via QFutureWatcher<BioSimResult>.
    QFutureWatcher<BioSimResult>* watcher_{nullptr};
};

} // namespace beamlab::biosim
