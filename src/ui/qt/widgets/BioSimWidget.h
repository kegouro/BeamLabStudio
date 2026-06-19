#pragma once

#include "domain/simulation/SimulationEngine.h"
#include "domain/materials/MaterialRegistry.h"
#include "ui/qt/widgets/BioViewport3D.h"
#include "ui/qt/widgets/EnergyColorMapper.h"
#include "ui/qt/widgets/EnergyScaleBar.h"
#include "ui/qt/widgets/SlabEditor3D.h"
#include "ui/qt/widgets/TrajectoryInspectorPanel.h"

#include <QFutureWatcher>
#include <QWidget>

class QAction;
class QComboBox;
class QLabel;
class QProgressBar;
class QPushButton;
class QSplitter;
class QToolBar;

namespace beamlab::domain::simulation {
struct BioSimConfig;
struct BioSimResult;
} // namespace beamlab::domain::simulation

namespace beamlab::ui {

class BioSimWidget : public QWidget {
    Q_OBJECT

public:
    explicit BioSimWidget(
        domain::simulation::SimulationEngine* engine,
        domain::materials::MaterialRegistry* materialRegistry,
        QWidget* parent = nullptr);

    void setCsvPath(const QString& path);

signals:
    void simulationFinished(bool success, const QString& message);

private slots:
    void onBrowseCsv();
    void onRun();
    void onStop();
    void onSimFinished();
    void onStepPicked(int track_idx, int step_idx);
    void onSlabsChanged(const std::vector<domain::geometry::Slab>& slabs);
    void onColorByChanged(int index);
    void onPaletteChanged(int index);
    void onColorRangeChanged(double min_val, double max_val, const QString& unit);
    void onOpenMaterialEditor();
    void onOpenPhantomPanel();

private:
    void buildLayout();
    void buildToolbar(QToolBar* tb);
    domain::simulation::BioSimConfig buildConfig() const;

    domain::simulation::SimulationEngine* engine_;
    domain::materials::MaterialRegistry* materialRegistry_;

    BioViewport3D*           viewport_{nullptr};
    EnergyScaleBar*          scale_bar_{nullptr};
    TrajectoryInspectorPanel* inspector_{nullptr};
    SlabEditor3D*            slab_editor_{nullptr};

    QLabel*       csv_label_{nullptr};
    QComboBox*    color_by_combo_{nullptr};
    QComboBox*    palette_combo_{nullptr};
    QPushButton*  btn_run_{nullptr};
    QPushButton*  btn_stop_{nullptr};
    QLabel*       status_label_{nullptr};
    QProgressBar* progress_bar_{nullptr};

    QString       csv_path_{};
    domain::simulation::BioSimResult last_result_{};
    bool          running_{false};
    QFutureWatcher<domain::simulation::BioSimResult>* watcher_{nullptr};
};

} // namespace beamlab::ui
