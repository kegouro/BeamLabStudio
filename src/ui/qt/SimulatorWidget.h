#pragma once

#include "io/normalization/AxisFrameResolver.h"
#include "simulation/physics/MuonTrackSimulator.h"
#include "simulation/tissue/TissueRegistry.h"

#include <QWidget>
#include <QString>
#include <vector>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QGraphicsScene;
class QGroupBox;
class QLabel;
class QPushButton;
class QSpinBox;
class QTableWidget;
class QTabWidget;

namespace beamlab::ui {

class InteractiveGraphicsView;

// Simulator tab: allows the user to place virtual tissue slabs over loaded
// trajectory data, run the Bethe-Bloch energy-loss model, and visualise the
// resulting dose and scoring-plane statistics.
//
// Workflow:
//   1. User loads a run (manifest) from the main dashboard (path forwarded here).
//   2. Tissue slabs are added via the controls panel.
//   3. "Run Simulation" triggers the MuonTrackSimulator in a background thread.
//   4. Results (dose map, scoring planes, energy profile plots) are displayed.
class SimulatorWidget final : public QWidget {
    Q_OBJECT

public:
    explicit SimulatorWidget(QWidget* parent = nullptr);

    // Called by MainWindow when a new run manifest is loaded.
    void setRunDirectory(const QString& run_dir);

public slots:
    void runSimulation();
    void addTissueSlab();
    void removeTissueSlab();
    void toggleSlabEnabled();
    void clearAllSlabs();
    void detectScoringPlanes();
    void addScoringPlane();
    void removeScoringPlane();
    void toggleScoringPlaneEnabled();
    void exportSimulationResults();

signals:
    void simulationFinished(bool success, const QString& message);

private:
    void buildUi();
    void buildSlabControls(QWidget* parent_widget);
    void buildScoringControls(QWidget* parent_widget);
    void buildResultsPanel(QWidget* parent_widget);
    void refreshSlabTable();
    void refreshScoringTable();
    void plotEnergyProfile(const QString& csv_path);
    void updateStatusLabel(const QString& text);

    // Slab data (lightweight mirror of simulation::TissueSlab for UI)
    struct SlabRow {
        QString id{};
        QString material{};
        double density_g_cm3{1.0};
        double i_eV{75.0};
        double start_m{0.0};
        double thickness_m{0.01};
        bool enabled{true};
    };

    // Scoring plane data (mirror of simulation::ScoringPlane for UI)
    struct ScoringRow {
        QString id{};
        QString role{"counter"};
        double axial_m{0.0};
        bool enabled{true};
        std::size_t crossings{0};
        double mean_kinE_MeV{0.0};
    };

    std::vector<SlabRow> slabs_{};
    std::vector<ScoringRow> scoring_planes_{};
    QString run_dir_{};
    std::size_t slab_counter_{0};
    std::size_t scoring_counter_{0};

    // Slab controls
    QTableWidget* slab_table_{nullptr};
    QComboBox* material_combo_{nullptr};
    QDoubleSpinBox* density_spin_{nullptr};
    QDoubleSpinBox* i_eV_spin_{nullptr};
    QDoubleSpinBox* slab_start_spin_{nullptr};
    QDoubleSpinBox* slab_thickness_spin_{nullptr};
    QPushButton* add_slab_button_{nullptr};
    QPushButton* remove_slab_button_{nullptr};
    QPushButton* toggle_slab_button_{nullptr};
    QPushButton* clear_slabs_button_{nullptr};

    // Scoring controls
    QTableWidget* scoring_table_{nullptr};
    QDoubleSpinBox* scoring_pos_spin_{nullptr};
    QComboBox* scoring_role_combo_{nullptr};
    QPushButton* add_scoring_button_{nullptr};
    QPushButton* remove_scoring_button_{nullptr};
    QPushButton* toggle_scoring_button_{nullptr};
    QPushButton* detect_scoring_button_{nullptr};

    // Results
    QTabWidget* results_tabs_{nullptr};
    InteractiveGraphicsView* energy_plot_view_{nullptr};
    QGraphicsScene* energy_scene_{nullptr};
    QTableWidget* dose_table_{nullptr};
    QTableWidget* scoring_results_table_{nullptr};

    // Controls
    QPushButton* run_button_{nullptr};
    QPushButton* export_button_{nullptr};
    QLabel* status_label_{nullptr};

    // Physics backend (lives for the widget's lifetime; thread-safe to call from main thread)
    beamlab::simulation::MuonTrackSimulator physics_engine_{};
    beamlab::simulation::TissueRegistry tissue_registry_{};
    beamlab::io::AxisFrameResolver axis_resolver_{};
};

} // namespace beamlab::ui
