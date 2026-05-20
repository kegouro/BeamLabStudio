#pragma once

#include "analysis/statistics/FrameStatistics.h"
#include "analysis/statistics/FrameStatisticsParameters.h"

#include <cstdint>
#include <vector>

namespace beamlab::core {
class ISampleStorage;
} // namespace beamlab::core

namespace beamlab::data {
struct AxisFrame;
} // namespace beamlab::data

namespace beamlab::analysis {

class BatchStatisticsEngine {
public:
    [[nodiscard]] std::vector<FrameStatistics> compute(
        const beamlab::core::ISampleStorage& storage,
        const beamlab::data::AxisFrame& axisFrame,
        const FrameStatisticsParameters& parameters = {}) const;

private:
    [[nodiscard]] std::vector<FrameStatistics> computeUniformBins(
        const beamlab::core::ISampleStorage& storage,
        const beamlab::data::AxisFrame& axisFrame,
        std::size_t binCount,
        double sMin, double sMax) const;
};

} // namespace beamlab::analysis
