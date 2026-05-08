#pragma once

#include "data/model/TrajectoryDataset.h"
#include "simulation/physics/BetheBlochEngine.h"
#include "simulation/tissue/TissueSlab.h"

#include <vector>

namespace beamlab::simulation {

// Result of applying tissue slabs to a trajectory dataset.
// The original dataset is unchanged; this carries the modified copy.
struct SimulationResult {
    beamlab::data::TrajectoryDataset dataset{};
    bool valid{false};
    std::string message{};

    // Per-slab energy deposition statistics (summed over all tracks)
    struct SlabStats {
        std::string slab_id{};
        double total_edep_MeV{0.0};
        double mean_edep_per_track_MeV{0.0};
        double mean_dose_Gy{0.0};
        std::size_t tracks_crossing{0};
    };
    std::vector<SlabStats> slab_stats{};
};

// Re-calculates energy loss for each trajectory step that falls inside any
// enabled TissueSlab, replacing the sample's edep_MeV, edep_eV and dose_Gy
// with values derived from the Bethe-Bloch stopping power model.
//
// Steps outside all slabs are treated as vacuum (edep = 0).
// The kinetic energy is propagated from step to step so that subsequent
// slabs see the correct residual energy.
class MuonTrackSimulator {
public:
    [[nodiscard]] SimulationResult simulate(
        const beamlab::data::TrajectoryDataset& dataset,
        const std::vector<TissueSlab>& slabs) const;

private:
    BetheBlochEngine physics_{};
};

} // namespace beamlab::simulation
