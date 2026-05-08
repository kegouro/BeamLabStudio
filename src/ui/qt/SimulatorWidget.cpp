#include "ui/qt/SimulatorWidget.h"
#include "ui/qt/InteractiveGraphicsView.h"

#include "data/ids/SampleId.h"
#include "data/ids/TrajectoryId.h"
#include "data/model/DatasetMetadata.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileInfo>
#include <QFrame>
#include <QGraphicsScene>
#include <QGraphicsTextItem>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QSplitter>
#include <QTableWidget>
#include <QTabWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>

namespace beamlab::ui {

SimulatorWidget::SimulatorWidget(QWidget* parent)
    : QWidget(parent)
{
    buildUi();
}

void SimulatorWidget::buildUi()
{
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(6, 6, 6, 6);
    root->setSpacing(6);

    // ── Left panel: scrollable controls ──────────────────────────────────────
    auto* ctrl_widget = new QWidget();
    auto* ctrl_layout = new QVBoxLayout(ctrl_widget);
    ctrl_layout->setContentsMargins(4, 4, 4, 4);
    ctrl_layout->setSpacing(6);

    buildSlabControls(ctrl_widget);
    buildScoringControls(ctrl_widget);

    // Run / export buttons
    auto* action_group = new QGroupBox("Simulation");
    auto* action_layout = new QVBoxLayout(action_group);

    run_button_ = new QPushButton("Run Simulation");
    run_button_->setToolTip("Apply tissue slabs to all loaded trajectories using "
                             "the Bethe-Bloch stopping-power model");
    run_button_->setEnabled(false);

    export_button_ = new QPushButton("Export Results");
    export_button_->setToolTip("Save energy profiles, dose summary and scoring-plane "
                                "statistics to the current run output directory");
    export_button_->setEnabled(false);

    status_label_ = new QLabel("Load a run from the Dashboard first.");
    status_label_->setWordWrap(true);
    status_label_->setStyleSheet("color: #888;");

    action_layout->addWidget(run_button_);
    action_layout->addWidget(export_button_);
    action_layout->addWidget(status_label_);

    ctrl_layout->addWidget(action_group);
    ctrl_layout->addStretch();

    // Wrap the left panel in a scroll area so it is scrollable when the
    // window is too short to display all controls at once.
    auto* ctrl_scroll = new QScrollArea();
    ctrl_scroll->setWidget(ctrl_widget);
    ctrl_scroll->setWidgetResizable(true);
    ctrl_scroll->setFrameShape(QFrame::NoFrame);
    ctrl_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ctrl_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    ctrl_scroll->setMinimumWidth(340);
    ctrl_scroll->setMaximumWidth(440);
    ctrl_scroll->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);

    // ── Right panel: results ──────────────────────────────────────────────────
    auto* results_widget = new QWidget();
    buildResultsPanel(results_widget);

    // ── Splitter ──────────────────────────────────────────────────────────────
    auto* splitter = new QSplitter(Qt::Horizontal);
    splitter->addWidget(ctrl_scroll);
    splitter->addWidget(results_widget);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);

    root->addWidget(splitter);

    // Connections
    connect(run_button_,    &QPushButton::clicked, this, &SimulatorWidget::runSimulation);
    connect(export_button_, &QPushButton::clicked, this, &SimulatorWidget::exportSimulationResults);
}

void SimulatorWidget::buildSlabControls(QWidget* parent_widget)
{
    auto* parent_layout = qobject_cast<QVBoxLayout*>(parent_widget->layout());

    auto* group = new QGroupBox("Tissue Slabs");
    auto* layout = new QVBoxLayout(group);

    // Slab table
    slab_table_ = new QTableWidget(0, 6);
    slab_table_->setHorizontalHeaderLabels(
        {"Material", "ρ g/cm³", "I (eV)", "Start (m)", "Thick (m)", "On"});
    slab_table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    slab_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    slab_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    slab_table_->setMaximumHeight(180);
    layout->addWidget(slab_table_);

    // Controls row
    // Material selector
    auto* row1 = new QHBoxLayout();
    material_combo_ = new QComboBox();
    material_combo_->addItems({"Water", "Muscle", "Cortical Bone",
                               "Adipose Tissue", "Brain", "Lung (inflated)", "Air"});
    row1->addWidget(new QLabel("Material:"));
    row1->addWidget(material_combo_, 1);
    layout->addLayout(row1);

    // Material coefficient overrides (ρ and I_eV)
    auto* coeff_row = new QHBoxLayout();
    density_spin_ = new QDoubleSpinBox();
    density_spin_->setDecimals(4);
    density_spin_->setRange(0.0001, 20.0);
    density_spin_->setSuffix(" g/cm³");
    density_spin_->setValue(1.0);
    density_spin_->setToolTip("Mass density — override for Bethe-Bloch dE/dx calculation");

    i_eV_spin_ = new QDoubleSpinBox();
    i_eV_spin_->setDecimals(1);
    i_eV_spin_->setRange(10.0, 1000.0);
    i_eV_spin_->setSuffix(" eV");
    i_eV_spin_->setValue(75.0);
    i_eV_spin_->setToolTip("Mean excitation energy I (ICRU 44) — override for Bethe-Bloch");

    coeff_row->addWidget(new QLabel("ρ:"));
    coeff_row->addWidget(density_spin_);
    coeff_row->addWidget(new QLabel("I:"));
    coeff_row->addWidget(i_eV_spin_);
    layout->addLayout(coeff_row);

    // Position / thickness
    auto* row2 = new QHBoxLayout();
    slab_start_spin_ = new QDoubleSpinBox();
    slab_start_spin_->setDecimals(4);
    slab_start_spin_->setRange(-1000.0, 1000.0);
    slab_start_spin_->setSuffix(" m");
    slab_start_spin_->setValue(11.60);

    slab_thickness_spin_ = new QDoubleSpinBox();
    slab_thickness_spin_->setDecimals(4);
    slab_thickness_spin_->setRange(0.0001, 100.0);
    slab_thickness_spin_->setSuffix(" m");
    slab_thickness_spin_->setValue(0.05);

    row2->addWidget(new QLabel("Start:"));
    row2->addWidget(slab_start_spin_);
    row2->addWidget(new QLabel("Thick:"));
    row2->addWidget(slab_thickness_spin_);
    layout->addLayout(row2);

    auto* btn_row = new QHBoxLayout();
    add_slab_button_    = new QPushButton("Add");
    remove_slab_button_ = new QPushButton("Remove");
    toggle_slab_button_ = new QPushButton("Toggle");
    clear_slabs_button_ = new QPushButton("Clear All");
    btn_row->addWidget(add_slab_button_);
    btn_row->addWidget(remove_slab_button_);
    btn_row->addWidget(toggle_slab_button_);
    btn_row->addWidget(clear_slabs_button_);
    layout->addLayout(btn_row);

    parent_layout->addWidget(group);

    connect(add_slab_button_,    &QPushButton::clicked, this, &SimulatorWidget::addTissueSlab);
    connect(remove_slab_button_, &QPushButton::clicked, this, &SimulatorWidget::removeTissueSlab);
    connect(toggle_slab_button_, &QPushButton::clicked, this, &SimulatorWidget::toggleSlabEnabled);
    connect(clear_slabs_button_, &QPushButton::clicked, this, &SimulatorWidget::clearAllSlabs);

    // Auto-populate density and I_eV when the material selection changes
    connect(material_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx) {
                static const QList<double> dens = {1.0, 1.05, 1.92, 0.95, 1.04, 0.26, 0.001205};
                static const QList<double> i_ev = {75.0, 74.0, 106.4, 63.2, 73.3, 75.3, 85.7};
                density_spin_->setValue(dens.value(idx, 1.0));
                i_eV_spin_->setValue(i_ev.value(idx, 75.0));
            });
}

void SimulatorWidget::buildScoringControls(QWidget* parent_widget)
{
    auto* parent_layout = qobject_cast<QVBoxLayout*>(parent_widget->layout());

    auto* group = new QGroupBox("Scoring Planes / Particle Counters");
    auto* layout = new QVBoxLayout(group);

    scoring_table_ = new QTableWidget(0, 4);
    scoring_table_->setHorizontalHeaderLabels({"ID", "Role", "Axial (m)", "On"});
    scoring_table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    scoring_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    scoring_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    scoring_table_->setMaximumHeight(150);
    layout->addWidget(scoring_table_);

    auto* row1 = new QHBoxLayout();
    scoring_pos_spin_ = new QDoubleSpinBox();
    scoring_pos_spin_->setDecimals(4);
    scoring_pos_spin_->setRange(-1000.0, 1000.0);
    scoring_pos_spin_->setSuffix(" m");
    scoring_pos_spin_->setValue(11.60);

    scoring_role_combo_ = new QComboBox();
    scoring_role_combo_->addItems({"counter", "entry", "exit"});

    row1->addWidget(new QLabel("Axial:"));
    row1->addWidget(scoring_pos_spin_);
    row1->addWidget(new QLabel("Role:"));
    row1->addWidget(scoring_role_combo_);
    layout->addLayout(row1);

    auto* btn_row = new QHBoxLayout();
    add_scoring_button_    = new QPushButton("Add");
    remove_scoring_button_ = new QPushButton("Remove");
    toggle_scoring_button_ = new QPushButton("Toggle");
    detect_scoring_button_ = new QPushButton("Auto-Detect");
    detect_scoring_button_->setToolTip(
        "Automatically detect beam entry, exit and scoring planes "
        "from the trajectory density profile");
    btn_row->addWidget(add_scoring_button_);
    btn_row->addWidget(remove_scoring_button_);
    btn_row->addWidget(toggle_scoring_button_);
    btn_row->addWidget(detect_scoring_button_);
    layout->addLayout(btn_row);

    parent_layout->addWidget(group);

    connect(add_scoring_button_,    &QPushButton::clicked, this, &SimulatorWidget::addScoringPlane);
    connect(remove_scoring_button_, &QPushButton::clicked, this, &SimulatorWidget::removeScoringPlane);
    connect(toggle_scoring_button_, &QPushButton::clicked, this, &SimulatorWidget::toggleScoringPlaneEnabled);
    connect(detect_scoring_button_, &QPushButton::clicked, this, &SimulatorWidget::detectScoringPlanes);
}

void SimulatorWidget::buildResultsPanel(QWidget* parent_widget)
{
    auto* layout = new QVBoxLayout(parent_widget);
    layout->setContentsMargins(0, 0, 0, 0);

    results_tabs_ = new QTabWidget();

    // Tab 1: Energy profile plot (InteractiveGraphicsView already handles pan/zoom)
    auto* plot_tab = new QWidget();
    auto* plot_layout = new QVBoxLayout(plot_tab);
    plot_layout->setContentsMargins(0, 0, 0, 0);
    energy_scene_ = new QGraphicsScene(this);
    energy_plot_view_ = new InteractiveGraphicsView(energy_scene_);
    energy_plot_view_->setMinimumHeight(200);
    plot_layout->addWidget(energy_plot_view_);
    results_tabs_->addTab(plot_tab, "Energy Profile");

    // Tab 2: Dose table (per slab) — QTableWidget is inherently scrollable
    dose_table_ = new QTableWidget(0, 6);
    dose_table_->setHorizontalHeaderLabels(
        {"Slab", "Material", "ρ g/cm³", "Total edep MeV",
         "Mean Dose Gy", "Tracks"});
    dose_table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    dose_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    results_tabs_->addTab(dose_table_, "Slab Dose");

    // Tab 3: Scoring plane results — also inherently scrollable
    scoring_results_table_ = new QTableWidget(0, 5);
    scoring_results_table_->setHorizontalHeaderLabels(
        {"ID", "Role", "Axial m", "Crossings", "Mean KinE MeV"});
    scoring_results_table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    scoring_results_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    results_tabs_->addTab(scoring_results_table_, "Scoring Planes");

    layout->addWidget(results_tabs_, 1);
}

// ── Slots ───────────────────────────────────────────────────────────────────

void SimulatorWidget::setRunDirectory(const QString& run_dir)
{
    run_dir_ = run_dir;
    run_button_->setEnabled(!run_dir_.isEmpty());
    detect_scoring_button_->setEnabled(!run_dir_.isEmpty());
    updateStatusLabel("Run loaded: " + QFileInfo(run_dir).fileName());

    // Try to display an existing energy profile if already computed
    const QString profile_path =
        run_dir + "/tables/energy_step_profile.csv";
    if (QFileInfo::exists(profile_path)) {
        plotEnergyProfile(profile_path);
    }

    // Load existing scoring planes if present
    const QString sp_path = run_dir + "/tables/scoring_planes.csv";
    if (QFileInfo::exists(sp_path)) {
        std::ifstream f(sp_path.toStdString());
        std::string line;
        scoring_planes_.clear();
        while (std::getline(f, line)) {
            if (line.empty() || line[0] == '#') continue;
            if (line.find("id,role") != std::string::npos) continue;
            std::istringstream ss(line);
            std::string tok;
            ScoringRow row{};
            int col = 0;
            while (std::getline(ss, tok, ',')) {
                switch (col++) {
                case 0: row.id = QString::fromStdString(tok); break;
                case 1: row.role = QString::fromStdString(tok); break;
                case 2: try { row.axial_m = std::stod(tok); } catch (...) {} break;
                case 3: row.enabled = (tok == "true"); break;
                case 4: try { row.crossings = static_cast<std::size_t>(std::stoul(tok)); } catch (...) {} break;
                case 5: try { row.mean_kinE_MeV = std::stod(tok); } catch (...) {} break;
                }
            }
            if (!row.id.isEmpty()) {
                scoring_planes_.push_back(row);
            }
        }
        refreshScoringTable();
    }
}

void SimulatorWidget::addTissueSlab()
{
    SlabRow row{};
    row.id            = QString("slab_%1").arg(slab_counter_++);
    row.material      = material_combo_->currentText();
    row.density_g_cm3 = density_spin_->value();
    row.i_eV          = i_eV_spin_->value();
    row.start_m       = slab_start_spin_->value();
    row.thickness_m   = slab_thickness_spin_->value();
    row.enabled       = true;
    slabs_.push_back(row);
    refreshSlabTable();
}

void SimulatorWidget::removeTissueSlab()
{
    const auto rows = slab_table_->selectedItems();
    if (rows.isEmpty()) return;
    const int row = slab_table_->currentRow();
    if (row < 0 || static_cast<std::size_t>(row) >= slabs_.size()) return;
    slabs_.erase(slabs_.begin() + row);
    refreshSlabTable();
}

void SimulatorWidget::toggleSlabEnabled()
{
    const int row = slab_table_->currentRow();
    if (row < 0 || static_cast<std::size_t>(row) >= slabs_.size()) return;
    slabs_[static_cast<std::size_t>(row)].enabled =
        !slabs_[static_cast<std::size_t>(row)].enabled;
    refreshSlabTable();
}

void SimulatorWidget::clearAllSlabs()
{
    slabs_.clear();
    refreshSlabTable();
}

void SimulatorWidget::addScoringPlane()
{
    ScoringRow row{};
    row.id      = QString("plane_%1").arg(scoring_counter_++);
    row.role    = scoring_role_combo_->currentText();
    row.axial_m = scoring_pos_spin_->value();
    row.enabled = true;
    scoring_planes_.push_back(row);
    refreshScoringTable();
}

void SimulatorWidget::removeScoringPlane()
{
    const int row = scoring_table_->currentRow();
    if (row < 0 || static_cast<std::size_t>(row) >= scoring_planes_.size()) return;
    scoring_planes_.erase(scoring_planes_.begin() + row);
    refreshScoringTable();
}

void SimulatorWidget::toggleScoringPlaneEnabled()
{
    const int row = scoring_table_->currentRow();
    if (row < 0 || static_cast<std::size_t>(row) >= scoring_planes_.size()) return;
    scoring_planes_[static_cast<std::size_t>(row)].enabled =
        !scoring_planes_[static_cast<std::size_t>(row)].enabled;
    refreshScoringTable();
}

void SimulatorWidget::detectScoringPlanes()
{
    if (run_dir_.isEmpty()) {
        updateStatusLabel("No run loaded.");
        return;
    }

    const QString sp_path = run_dir_ + "/tables/scoring_planes.csv";
    if (!QFileInfo::exists(sp_path)) {
        updateStatusLabel("scoring_planes.csv not found. Run the CLI first with --detect-scoring-planes.");
        return;
    }

    // Re-load from disk
    setRunDirectory(run_dir_);
    updateStatusLabel("Scoring planes loaded from disk.");
}

void SimulatorWidget::runSimulation()
{
    if (run_dir_.isEmpty()) {
        updateStatusLabel("No run loaded.");
        return;
    }

    // ── 1. Load trajectory preview CSV into a minimal TrajectoryDataset ──────
    const QString preview_csv =
        run_dir_ + "/visualization/trajectories_preview.csv";

    if (!QFileInfo::exists(preview_csv)) {
        updateStatusLabel("trajectories_preview.csv not found. Load a run first.");
        return;
    }

    updateStatusLabel("Running simulation…");

    beamlab::data::TrajectoryDataset dataset{};
    {
        std::ifstream f(preview_csv.toStdString());
        std::string line;
        bool header_skipped = false;
        std::map<int, beamlab::data::Trajectory*> traj_map;

        while (std::getline(f, line)) {
            if (line.empty() || line[0] == '#') continue;
            if (!header_skipped) { header_skipped = true; continue; }

            std::istringstream ss(line);
            std::string tok;
            std::vector<std::string> cols;
            while (std::getline(ss, tok, ',')) { cols.push_back(tok); }
            if (cols.size() < 13) continue;

            try {
                const int ti = std::stoi(cols[0]);
                const int si = std::stoi(cols[1]);

                if (traj_map.find(ti) == traj_map.end()) {
                    beamlab::data::Trajectory traj{};
                    traj.id = beamlab::data::TrajectoryId(static_cast<uint64_t>(ti + 1));
                    traj.particle.particle_type = "mu-";
                    traj.particle.charge = -1.0;
                    traj.particle.mass_MeV = 105.6583755;
                    dataset.trajectories.push_back(std::move(traj));
                    traj_map[ti] = &dataset.trajectories.back();
                }

                beamlab::data::TrajectorySample sample{};
                sample.id = beamlab::data::SampleId(static_cast<uint64_t>(si + 1));
                sample.trajectory_id = traj_map[ti]->id;
                sample.time_s         = std::stod(cols[2]);
                sample.position_m.x   = std::stod(cols[3]);
                sample.position_m.y   = std::stod(cols[4]);
                sample.position_m.z   = std::stod(cols[5]);
                sample.edep_MeV       = std::stod(cols[6]);
                sample.edep_eV        = std::stod(cols[7]);
                sample.kinE_MeV       = std::stod(cols[8]);
                sample.momentum_MeV.x = std::stod(cols[9]);
                sample.momentum_MeV.y = std::stod(cols[10]);
                sample.momentum_MeV.z = std::stod(cols[11]);
                sample.dose_Gy        = std::stod(cols[12]);

                traj_map[ti]->samples.push_back(sample);

                // Set initial kinE from first sample
                if (si == 0) {
                    traj_map[ti]->particle.initial_kinE_MeV = sample.kinE_MeV;
                }
            } catch (...) {}
        }
    }

    if (dataset.trajectories.empty()) {
        updateStatusLabel("No trajectory data found in preview CSV.");
        return;
    }

    // ── 2. Auto-resolve beam axis frame ──────────────────────────────────────
    dataset.axis_frame = axis_resolver_.resolve(dataset);

    // ── 3. Build TissueSlab vector from UI rows ───────────────────────────────
    std::vector<beamlab::simulation::TissueSlab> slabs;
    slabs.reserve(slabs_.size());
    for (const auto& row : slabs_) {
        if (!row.enabled) continue;
        beamlab::simulation::TissueSlab slab{};
        slab.id               = row.id.toStdString();
        slab.axial_start_m    = row.start_m;
        slab.thickness_m      = row.thickness_m;
        slab.enabled          = true;
        slab.material.name    = row.material.toStdString();
        slab.material.density_g_cm3 = row.density_g_cm3;
        slab.material.I_eV          = row.i_eV;
        // Look up Z_eff / A_eff from registry; fall back to water values
        const auto mat_opt = tissue_registry_.find(row.material.toLower().toStdString());
        if (mat_opt) {
            slab.material.Z_eff = mat_opt->Z_eff;
            slab.material.A_eff = mat_opt->A_eff;
            slab.material.symbol = mat_opt->symbol;
            slab.material.radiation_length_cm = mat_opt->radiation_length_cm;
        } else {
            slab.material.Z_eff  = 7.22;
            slab.material.A_eff  = 12.01;
        }
        slabs.push_back(slab);
    }

    // ── 4. Run Bethe-Bloch simulation ─────────────────────────────────────────
    const auto result = physics_engine_.simulate(dataset, slabs);

    if (!result.valid) {
        updateStatusLabel("Simulation failed: " +
                          QString::fromStdString(result.message));
        return;
    }

    // ── 5. Update dose table ──────────────────────────────────────────────────
    dose_table_->setRowCount(0);
    for (const auto& st : result.slab_stats) {
        const int row = dose_table_->rowCount();
        dose_table_->insertRow(row);
        // Find matching UI row for material name
        QString mat_name;
        double density = 0.0;
        for (const auto& sr : slabs_) {
            if (sr.id.toStdString() == st.slab_id) {
                mat_name = sr.material;
                density  = sr.density_g_cm3;
                break;
            }
        }
        dose_table_->setItem(row, 0, new QTableWidgetItem(
            QString::fromStdString(st.slab_id)));
        dose_table_->setItem(row, 1, new QTableWidgetItem(mat_name));
        dose_table_->setItem(row, 2, new QTableWidgetItem(
            QString::number(density, 'f', 4)));
        dose_table_->setItem(row, 3, new QTableWidgetItem(
            QString::number(st.total_edep_MeV, 'e', 4)));
        dose_table_->setItem(row, 4, new QTableWidgetItem(
            QString::number(st.mean_dose_Gy, 'e', 4)));
        dose_table_->setItem(row, 5, new QTableWidgetItem(
            QString::number(static_cast<qulonglong>(st.tracks_crossing))));
    }

    // ── 6. Score the simulated dataset at each scoring plane ──────────────────
    const auto& frame = dataset.axis_frame;
    const auto axialOf = [&](const beamlab::data::TrajectorySample& s) {
        return (s.position_m.x - frame.origin.x) * frame.longitudinal.x +
               (s.position_m.y - frame.origin.y) * frame.longitudinal.y +
               (s.position_m.z - frame.origin.z) * frame.longitudinal.z;
    };

    for (auto& sp : scoring_planes_) {
        if (!sp.enabled) continue;
        sp.crossings     = 0;
        sp.mean_kinE_MeV = 0.0;
        double kinE_sum  = 0.0;

        for (const auto& traj : result.dataset.trajectories) {
            for (std::size_t i = 0; i + 1 < traj.samples.size(); ++i) {
                const double a0 = axialOf(traj.samples[i]);
                const double a1 = axialOf(traj.samples[i + 1]);
                if ((a0 <= sp.axial_m && a1 >= sp.axial_m) ||
                    (a0 >= sp.axial_m && a1 <= sp.axial_m)) {
                    ++sp.crossings;
                    kinE_sum += traj.samples[i].kinE_MeV;
                    break; // count each trajectory once per plane
                }
            }
        }
        if (sp.crossings > 0) {
            sp.mean_kinE_MeV = kinE_sum / static_cast<double>(sp.crossings);
        }
    }
    refreshScoringTable();

    // ── 7. Draw inline energy profile from simulation result ──────────────────
    energy_scene_->clear();
    {
        struct Pt { double axial{}; double kinE{}; };
        std::vector<Pt> pts;
        pts.reserve(result.dataset.trajectories.size() * 20);

        for (const auto& traj : result.dataset.trajectories) {
            for (const auto& s : traj.samples) {
                const double ax =
                    s.position_m.x * dataset.axis_frame.longitudinal.x +
                    s.position_m.y * dataset.axis_frame.longitudinal.y +
                    s.position_m.z * dataset.axis_frame.longitudinal.z;
                pts.push_back({ax, s.kinE_MeV});
            }
        }

        if (!pts.empty()) {
            double axmin = pts[0].axial, axmax = pts[0].axial, emax = 0.0;
            for (const auto& p : pts) {
                axmin = std::min(axmin, p.axial);
                axmax = std::max(axmax, p.axial);
                emax  = std::max(emax, p.kinE);
            }

            constexpr double W = 780.0, H = 380.0, M = 30.0;
            energy_scene_->addLine(M, M, M, H + M, QPen(Qt::gray, 1));
            energy_scene_->addLine(M, H + M, W + M, H + M, QPen(Qt::gray, 1));

            // Draw slab regions as translucent overlays
            if (axmax > axmin) {
                for (const auto& sr : slabs_) {
                    if (!sr.enabled) continue;
                    const double x0 = M + (sr.start_m - axmin) / (axmax - axmin) * W;
                    const double x1 = M + (sr.start_m + sr.thickness_m - axmin)
                                          / (axmax - axmin) * W;
                    energy_scene_->addRect(x0, M, x1 - x0, H,
                        QPen(Qt::NoPen),
                        QBrush(QColor(220, 180, 80, 40)));
                }
            }

            // Plot points (subsampled)
            const std::size_t stride = std::max(std::size_t{1}, pts.size() / 8000);
            for (std::size_t i = 0; i < pts.size(); i += stride) {
                if (!(axmax > axmin) || !(emax > 0.0)) continue;
                const double x = M + (pts[i].axial - axmin) / (axmax - axmin) * W;
                const double y = H + M - pts[i].kinE / emax * H;
                energy_scene_->addEllipse(x - 1.0, y - 1.0, 2.0, 2.0,
                    QPen(Qt::NoPen), QBrush(QColor(60, 180, 100, 120)));
            }

            auto* title = energy_scene_->addText(
                QString("Simulated KinE — %1 track(s), %2 slab(s)")
                    .arg(result.dataset.trajectories.size())
                    .arg(slabs.size()));
            title->setDefaultTextColor(Qt::white);
            title->setPos(M, 2.0);

            auto* xl = energy_scene_->addText(
                QString("Axial [%1 .. %2 m]")
                    .arg(axmin, 0, 'e', 3).arg(axmax, 0, 'e', 3));
            xl->setDefaultTextColor(Qt::gray);
            xl->setPos(M, H + M + 4.0);

            auto* yl = energy_scene_->addText(
                QString("KinE [0 .. %1 MeV]").arg(emax, 0, 'f', 0));
            yl->setDefaultTextColor(Qt::gray);
            yl->setPos(2.0, M);

            energy_scene_->setSceneRect(0, 0, W + 2.0 * M, H + 2.0 * M + 20.0);
        }
    }

    results_tabs_->setCurrentIndex(0); // switch to energy plot tab

    export_button_->setEnabled(true);
    updateStatusLabel(
        QString("Simulation complete. %1 track(s), %2 slab(s) active.\n%3")
            .arg(result.dataset.trajectories.size())
            .arg(slabs.size())
            .arg(QString::fromStdString(result.message)));
}

void SimulatorWidget::exportSimulationResults()
{
    if (run_dir_.isEmpty()) {
        updateStatusLabel("No run loaded.");
        return;
    }

    std::filesystem::create_directories(
        (run_dir_ + "/simulation").toStdString());

    const QString slabs_cfg  = run_dir_ + "/simulation/tissue_slabs.csv";
    const QString scoring_cfg = run_dir_ + "/simulation/scoring_planes_user.csv";

    {
        std::ofstream out(slabs_cfg.toStdString());
        out << "id,material,density_g_cm3,i_eV,axial_start_m,thickness_m,enabled\n";
        for (const auto& s : slabs_) {
            out << s.id.toStdString() << ','
                << s.material.toStdString() << ','
                << s.density_g_cm3 << ','
                << s.i_eV << ','
                << s.start_m << ','
                << s.thickness_m << ','
                << (s.enabled ? "true" : "false") << '\n';
        }
    }

    {
        std::ofstream out(scoring_cfg.toStdString());
        out << "id,role,axial_position_m,enabled,crossings,mean_kinE_MeV\n";
        for (const auto& p : scoring_planes_) {
            out << p.id.toStdString() << ','
                << p.role.toStdString() << ','
                << p.axial_m << ','
                << (p.enabled ? "true" : "false") << ','
                << p.crossings << ','
                << p.mean_kinE_MeV << '\n';
        }
    }

    updateStatusLabel("Exported:\n" + slabs_cfg + "\n" + scoring_cfg);
}

// ── Private helpers ─────────────────────────────────────────────────────────

void SimulatorWidget::refreshSlabTable()
{
    slab_table_->setRowCount(0);
    for (const auto& s : slabs_) {
        const int row = slab_table_->rowCount();
        slab_table_->insertRow(row);
        slab_table_->setItem(row, 0, new QTableWidgetItem(s.material));
        slab_table_->setItem(row, 1, new QTableWidgetItem(
            QString::number(s.density_g_cm3, 'f', 4)));
        slab_table_->setItem(row, 2, new QTableWidgetItem(
            QString::number(s.i_eV, 'f', 1)));
        slab_table_->setItem(row, 3, new QTableWidgetItem(
            QString::number(s.start_m, 'f', 4)));
        slab_table_->setItem(row, 4, new QTableWidgetItem(
            QString::number(s.thickness_m, 'f', 4)));
        slab_table_->setItem(row, 5, new QTableWidgetItem(
            s.enabled ? "yes" : "no"));
    }
}

void SimulatorWidget::refreshScoringTable()
{
    scoring_table_->setRowCount(0);
    for (const auto& p : scoring_planes_) {
        const int row = scoring_table_->rowCount();
        scoring_table_->insertRow(row);
        scoring_table_->setItem(row, 0, new QTableWidgetItem(p.id));
        scoring_table_->setItem(row, 1, new QTableWidgetItem(p.role));
        scoring_table_->setItem(row, 2, new QTableWidgetItem(
            QString::number(p.axial_m, 'e', 4)));
        scoring_table_->setItem(row, 3, new QTableWidgetItem(
            p.enabled ? "yes" : "no"));
    }

    // Also update results table
    scoring_results_table_->setRowCount(0);
    for (const auto& p : scoring_planes_) {
        const int row = scoring_results_table_->rowCount();
        scoring_results_table_->insertRow(row);
        scoring_results_table_->setItem(row, 0, new QTableWidgetItem(p.id));
        scoring_results_table_->setItem(row, 1, new QTableWidgetItem(p.role));
        scoring_results_table_->setItem(row, 2, new QTableWidgetItem(
            QString::number(p.axial_m, 'e', 4)));
        scoring_results_table_->setItem(row, 3, new QTableWidgetItem(
            QString::number(static_cast<qulonglong>(p.crossings))));
        scoring_results_table_->setItem(row, 4, new QTableWidgetItem(
            QString::number(p.mean_kinE_MeV, 'f', 2)));
    }
}

void SimulatorWidget::plotEnergyProfile(const QString& csv_path)
{
    energy_scene_->clear();

    std::ifstream f(csv_path.toStdString());
    if (!f) {
        energy_scene_->addText("energy_step_profile.csv not found");
        return;
    }

    // Read axial_m and kinE_MeV columns
    // Header: event_id,track_id,step_index,axial_m,edep_MeV,edep_eV,kinE_MeV,dose_Gy,...
    struct Point { double axial{}; double kinE{}; double edep{}; };
    std::vector<Point> points{};
    points.reserve(1024);

    std::string line;
    bool header_found = false;
    int col_axial = 3, col_kinE = 6, col_edep = 4;

    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        if (!header_found) {
            header_found = true;
            continue; // skip header line
        }
        if (points.size() > 50000) break; // limit for UI rendering

        std::istringstream ss(line);
        std::string tok;
        std::vector<std::string> toks;
        while (std::getline(ss, tok, ',')) {
            toks.push_back(tok);
        }
        if (static_cast<int>(toks.size()) <= std::max(col_axial, col_kinE)) continue;

        try {
            Point p{};
            p.axial = std::stod(toks[static_cast<std::size_t>(col_axial)]);
            p.kinE  = std::stod(toks[static_cast<std::size_t>(col_kinE)]);
            p.edep  = std::stod(toks[static_cast<std::size_t>(col_edep)]);
            points.push_back(p);
        } catch (...) {}
    }

    if (points.empty()) {
        energy_scene_->addText("No data in energy profile.");
        return;
    }

    // Determine axis ranges
    double axial_min = points[0].axial, axial_max = points[0].axial;
    double kinE_max = 0.0;
    for (const auto& p : points) {
        axial_min = std::min(axial_min, p.axial);
        axial_max = std::max(axial_max, p.axial);
        kinE_max  = std::max(kinE_max, p.kinE);
    }

    if (!(axial_max > axial_min) || !(kinE_max > 0.0)) {
        energy_scene_->addText("Degenerate data range.");
        return;
    }

    // Simple scatter plot in scene coordinates [0..800 x 0..400]
    constexpr double W = 780.0, H = 380.0;
    constexpr double margin = 30.0;

    const auto toX = [&](double axial) {
        return margin + (axial - axial_min) / (axial_max - axial_min) * W;
    };
    const auto toY = [&](double kinE) {
        return H + margin - kinE / kinE_max * H;
    };

    // Draw axes
    energy_scene_->addLine(margin, margin, margin, H + margin,
        QPen(Qt::gray, 1));
    energy_scene_->addLine(margin, H + margin, W + margin, H + margin,
        QPen(Qt::gray, 1));

    // Plot kinetic energy as small dots (subsample for speed)
    const std::size_t stride = std::max(std::size_t{1}, points.size() / 8000);
    for (std::size_t i = 0; i < points.size(); i += stride) {
        const auto& p = points[i];
        const double x = toX(p.axial);
        const double y = toY(p.kinE);
        energy_scene_->addEllipse(x - 1.0, y - 1.0, 2.0, 2.0,
            QPen(Qt::NoPen), QBrush(QColor(60, 140, 220, 100)));
    }

    // Labels
    auto* title = energy_scene_->addText("Kinetic Energy vs Axial Position");
    title->setDefaultTextColor(Qt::white);
    title->setPos(margin, 2.0);

    auto* xl = energy_scene_->addText(
        QString("Axial [%1 .. %2 m]")
            .arg(axial_min, 0, 'e', 3).arg(axial_max, 0, 'e', 3));
    xl->setDefaultTextColor(Qt::gray);
    xl->setPos(margin, H + margin + 4.0);

    auto* yl = energy_scene_->addText(
        QString("KinE [0 .. %1 MeV]").arg(kinE_max, 0, 'f', 0));
    yl->setDefaultTextColor(Qt::gray);
    yl->setPos(2.0, margin);

    energy_scene_->setSceneRect(0, 0, W + 2.0 * margin, H + 2.0 * margin + 20.0);
}

void SimulatorWidget::updateStatusLabel(const QString& text)
{
    if (status_label_) {
        status_label_->setText(text);
    }
}

} // namespace beamlab::ui
