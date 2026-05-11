#pragma once

#include "biosim/core/BioSimConfig.h"
#include "biosim/core/BioSimResult.h"

#include <cstdint>
#include <functional>
#include <string>

namespace beamlab::biosim {

// Orchestrates the biological simulation pipeline.
//
// Usage (synchronous):
//   BioSimRunner runner;
//   auto result = runner.run(config);
//
// Usage (async with Qt — recommended for UI):
//   Wrap in a QThread or QtConcurrent::run() and connect to the
//   simulationFinished(BioSimResult) signal of BioSimWidget.
//
// Progress callback:
//   The optional progress_cb is called with an integer [0..100] as tracks
//   are processed. Return false from the callback to abort the run.
class BioSimRunner {
public:
    using ProgressCallback = std::function<bool(int percent)>;

    // Run the full simulation synchronously.
    // If progress_cb returns false, the run is aborted and result.valid = false.
    [[nodiscard]] BioSimResult run(const BioSimConfig& config,
                                    ProgressCallback progress_cb = nullptr) const;

private:
    // Load and parse energy_step_profile.csv into raw BioTrack/BioStep structures.
    // Returns false and populates error_msg if the file cannot be read.
    // Per-row drops (non-finite, negative energies, malformed rows) are
    // appended to warnings_out with a brief reason + first offending line
    // number, then summarised in BioSimResult.
    bool loadCsv(const std::string& path,
                 std::vector<BioTrack>& tracks,
                 std::string& error_msg,
                 std::vector<std::string>& warnings_out) const;

    // Apply Bethe-Bloch + straggling inside active slabs for a single track.
    // master_seed is the run-level master (BioSimResult::effective_seed) and
    // is combined with (event_id, track_id) via std::seed_seq so each track
    // gets a deterministic but independent RNG stream.
    // non_finite_eloss_counter is incremented every time a sampled energy
    // loss had to be clamped because it was negative or non-finite.
    void processTrack(BioTrack& track,
                      const BioSimConfig& config,
                      std::uint64_t master_seed,
                      std::size_t& non_finite_eloss_counter) const;

    // Accumulate per-slab statistics from all tracks.  warnings_out collects
    // non-fatal physics warnings (e.g. Bortfeld curve applied outside its
    // proton calibration domain).
    std::vector<BioSlabStats> buildSlabStats(const std::vector<BioTrack>& tracks,
                                              const BioSimConfig& config,
                                              std::vector<std::string>& warnings_out) const;

    // Compute global min/max for colormap scaling.
    void computeGlobalStats(BioSimResult& result) const;

    // SHA-256 (simplified Adler-32 fallback for portability — no OpenSSL dep).
    static std::string hashFile(const std::string& path);
};

} // namespace beamlab::biosim
