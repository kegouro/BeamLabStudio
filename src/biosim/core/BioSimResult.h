#pragma once

#include "biosim/core/EnergyScaleSet.h"
#include "biosim/physics/BraggPeakCalculator.h"

#include <cstdint>
#include <string>
#include <vector>

namespace beamlab::biosim {

// One simulation step — corresponds to one row in energy_step_profile.csv.
struct BioStep {
    int event_id{0};
    int track_id{0};
    int step_index{0};

    // 3D world position [m]
    double x_m{0.0};
    double y_m{0.0};
    double z_m{0.0};
    double axial_m{0.0};     // coordinate along the beam axis [m]

    EnergyScaleSet energy{};  // all physics and dosimetry scales for this step
};

// All steps of one particle track.
struct BioTrack {
    int event_id{0};
    int track_id{0};
    std::string particle_type{};   // from CSV metadata or BioSimConfig.particle

    std::vector<BioStep> steps{};

    // Summary per track (derived from steps)
    double total_edep_MeV_original{0.0};
    double total_edep_MeV_simulated{0.0};
    double entry_kinE_MeV{0.0};
    double exit_kinE_MeV{0.0};
    double total_dose_Gy{0.0};
    double total_H_mSv{0.0};
};

// Statistics accumulated for one slab across all tracks.
struct BioSlabStats {
    std::string slab_id{};
    std::string material_name{};
    double density_g_cm3{0.0};

    std::size_t tracks_crossing{0};
    std::size_t total_steps_in_slab{0};

    double total_edep_MeV_original{0.0};
    double total_edep_MeV_simulated{0.0};
    double mean_edep_per_track_MeV{0.0};

    double total_dose_Gy{0.0};
    double mean_dose_Gy{0.0};
    double total_H_mSv{0.0};
    double mean_H_mSv{0.0};
    double total_E_mSv{0.0};

    double mean_LET_keV_um{0.0};
    double mean_RBE{1.0};

    // Entry and exit kinetic energy statistics
    double mean_entry_kinE_MeV{0.0};
    double mean_exit_kinE_MeV{0.0};

    // Bragg peak curve for this slab (Bortfeld model, proton-optimised)
    std::vector<BraggPeakCalculator::BraggCurvePoint> bragg_curve{};
    double bragg_peak_depth_cm{0.0};  // CSDA range at mean entry energy
};

// Full result of one BioSimRunner::run() call.
struct BioSimResult {
    bool valid{false};
    std::string message{};

    // Per-track results (one per (event_id, track_id) pair in the CSV).
    std::vector<BioTrack> tracks{};

    // Per-slab aggregated statistics.
    std::vector<BioSlabStats> slab_stats{};

    // Global statistics (all steps, all tracks)
    std::size_t total_tracks{0};
    std::size_t total_steps{0};
    double global_min_kinE_MeV{0.0};
    double global_max_kinE_MeV{0.0};
    double global_min_edep_MeV{0.0};
    double global_max_edep_MeV{0.0};
    double global_min_LET_keV_um{0.0};
    double global_max_LET_keV_um{0.0};
    double global_min_dose_mGy{0.0};
    double global_max_dose_mGy{0.0};

    // Metadata
    std::string source_csv_sha256{};    // SHA-256 of the input CSV
    std::string biosim_version{"1.0"};
    std::string timestamp{};

    // Effective master seed used for the stochastic engines.  When the user
    // sets BioSimConfig::rng_seed = 0 we mint a fresh non-zero seed at run
    // time; recording it here lets the user reproduce the exact same outputs
    // by pasting the value back into rng_seed.
    std::uint64_t effective_seed{0};

    // Non-zero count when MuonTrackSimulator / BioSimRunner had to clamp a
    // non-finite or negative energy loss.  Surfacing it lets the UI flag
    // suspect runs without burying the diagnostic in logs.
    std::size_t non_finite_eloss_steps{0};

    // Non-fatal diagnostics surfaced from CSV loading and physics evaluation.
    // Populated by BioSimRunner::loadCsv (silenced rows) and processTrack
    // (e.g. Bortfeld curve applied outside its proton domain).
    std::vector<std::string> warnings{};
};

} // namespace beamlab::biosim
