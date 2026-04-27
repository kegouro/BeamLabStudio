#pragma once

#include "analysis/statistics/FrameStatistics.h"
#include "analysis/statistics/FrameStatisticsParameters.h"
#include "data/model/TrajectoryDataset.h"

#include <vector>

namespace beamlab::analysis {

class FrameStatisticsEngine {
public:
    [[nodiscard]] std::vector<FrameStatistics> compute(
        const beamlab::data::TrajectoryDataset& dataset) const;

    [[nodiscard]] std::vector<FrameStatistics> compute(
        const beamlab::data::TrajectoryDataset& dataset,
        const FrameStatisticsParameters& parameters) const;
};

} // namespace beamlab::analysis
