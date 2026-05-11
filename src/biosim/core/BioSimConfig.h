#pragma once

#include "biosim/geometry/BioSlab.h"
#include "biosim/physics/EnergyStraggling.h"
#include "biosim/physics/ParticleLibrary.h"

#include <cstdint>
#include <string>
#include <vector>

namespace beamlab::biosim {

// Complete, serialisable configuration for a biosim run.
// Passed to BioSimRunner::run() to produce a BioSimResult.
struct BioSimConfig {

    // ── Particle ──────────────────────────────────────────────────────────────
    ParticleSpecies particle{ParticleLibrary::muonMinus()};

    // ── Tissue geometry ───────────────────────────────────────────────────────
    std::vector<BioSlab> slabs{};

    // ── Physics options ───────────────────────────────────────────────────────
    StragglingMode straggling_mode{StragglingMode::Deterministic};
    bool apply_highland_mcs{false};    // apply MCS lateral displacement (approximate)
    bool compute_bragg_curves{true};   // compute Bortfeld curves for each slab

    // ── Dosimetry options ─────────────────────────────────────────────────────
    // Effective beam radius for dose calculation.
    // Default: 0.5642 cm  ≡ 1 cm² cross-section (r = √(1/π))
    double beam_radius_cm{0.5642};

    // ── Data source ───────────────────────────────────────────────────────────
    std::string energy_step_csv_path{};   // path to energy_step_profile.csv
    std::string run_directory{};          // top-level run output directory

    // ── Output options ────────────────────────────────────────────────────────
    std::string output_csv_path{};        // where to write the result CSV
    bool export_per_track_csv{false};     // one CSV file per track (can be large)

    // ── Metadata ──────────────────────────────────────────────────────────────
    std::string config_label{};           // user-defined label for this run
    // Master seed for the stochastic models (e.g. straggling).  0 means
    // "auto" — runtime mints a fresh non-zero seed and reports it in
    // BioSimResult::effective_seed so the user can replay the run later by
    // setting rng_seed to that value.
    std::uint64_t rng_seed{0};
};

} // namespace beamlab::biosim
