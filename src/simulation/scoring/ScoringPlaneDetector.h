#pragma once

#include "data/model/TrajectoryDataset.h"
#include "simulation/scoring/ScoringPlane.h"

#include <vector>

namespace beamlab::simulation {

struct ScoringDetectionParameters {
    // Number of histogram bins along the longitudinal axis
    std::size_t axial_bins{256};

    // A bin is considered a "scoring wall" (entry/exit boundary) if its
    // sample density drops from > high_threshold * peak to < low_threshold * peak
    double high_threshold{0.30}; // fraction of peak count
    double low_threshold{0.05};

    // Minimum number of trajectories that must cross a position to call it a boundary
    std::size_t min_crossings{10};
};

// Automatically detect particle counters, beam entries and exits from
// a trajectory dataset by analysing density changes along the longitudinal axis.
class ScoringPlaneDetector {
public:
    [[nodiscard]] std::vector<ScoringPlane> detect(
        const beamlab::data::TrajectoryDataset& dataset,
        const ScoringDetectionParameters& params = {}) const;
};

} // namespace beamlab::simulation
