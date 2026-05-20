#include "analysis/statistics/BatchStatisticsEngine.h"

#include "analysis/statistics/FrameStatisticsEngine.h"
#include "core/math/Vec3.h"
#include "core/storage/ISampleStorage.h"
#include "data/model/AxisFrame.h"
#include "data/model/TrajectorySample.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace beamlab::analysis {

namespace {

struct BatchAccumulator {
    uint64_t count{0};
    double sumS{0.0};
    double sumU{0.0};
    double sumV{0.0};
    double sumU2{0.0};
    double sumV2{0.0};
    double minS{std::numeric_limits<double>::infinity()};
    double maxS{-std::numeric_limits<double>::infinity()};
};

FrameStatistics finalizeBatch(const BatchAccumulator& acc)
{
    FrameStatistics stats{};
    if (acc.count == 0) return stats;

    const double inv = 1.0 / static_cast<double>(acc.count);
    stats.reference_value = acc.sumS * inv;
    stats.reference_min_value = acc.minS;
    stats.reference_max_value = acc.maxS;
    stats.point_count = static_cast<std::size_t>(acc.count);
    stats.centroid_u = acc.sumU * inv;
    stats.centroid_v = acc.sumV * inv;

    double varU = std::max(0.0, acc.sumU2 * inv - stats.centroid_u * stats.centroid_u);
    double varV = std::max(0.0, acc.sumV2 * inv - stats.centroid_v * stats.centroid_v);
    stats.sigma_u = std::sqrt(varU);
    stats.sigma_v = std::sqrt(varV);
    stats.r_rms = std::sqrt(varU + varV);

    if (!(stats.reference_max_value > stats.reference_min_value)) {
        const double eps = std::max(1.0e-12, std::abs(stats.reference_value) * 1.0e-9);
        stats.reference_min_value = stats.reference_value - eps;
        stats.reference_max_value = stats.reference_value + eps;
    }
    return stats;
}

} // namespace

std::vector<FrameStatistics> BatchStatisticsEngine::computeUniformBins(
    const beamlab::core::ISampleStorage& storage,
    const beamlab::data::AxisFrame& axisFrame,
    std::size_t binCount,
    double sMin, double sMax) const
{
    if (binCount == 0 || !(sMax > sMin)) return {};

    const double width = (sMax - sMin) / static_cast<double>(binCount);
    std::vector<BatchAccumulator> accumulators(binCount);

    constexpr uint64_t kBatchSize = 100000;
    uint64_t offset = 0;
    uint64_t total = storage.totalSampleCount();

    while (offset < total) {
        auto batch = storage.getBatch(offset, kBatchSize);
        for (const auto& sample : batch) {
            const auto relative = beamlab::core::subtract(sample.position_m, axisFrame.origin);
            const double s = beamlab::core::dot(relative, axisFrame.longitudinal);
            const double u = beamlab::core::dot(relative, axisFrame.transverse_u);
            const double v = beamlab::core::dot(relative, axisFrame.transverse_v);

            if (!std::isfinite(s) || !std::isfinite(u) || !std::isfinite(v)) continue;

            auto binIdx = static_cast<std::size_t>(std::floor((s - sMin) / width));
            if (binIdx >= binCount) binIdx = binCount - 1;

            auto& acc = accumulators[binIdx];
            ++acc.count;
            acc.sumS += s;
            acc.sumU += u;
            acc.sumV += v;
            acc.sumU2 += u * u;
            acc.sumV2 += v * v;
            acc.minS = std::min(acc.minS, s);
            acc.maxS = std::max(acc.maxS, s);
        }
        offset += batch.size();
        if (batch.empty()) break;
    }

    std::vector<FrameStatistics> results;
    results.reserve(binCount);
    for (std::size_t bin = 0; bin < binCount; ++bin) {
        if (accumulators[bin].count < 3) continue;
        auto stats = finalizeBatch(accumulators[bin]);
        stats.reference_min_value = sMin + static_cast<double>(bin) * width;
        stats.reference_max_value = stats.reference_min_value + width;
        stats.reference_value = 0.5 * (stats.reference_min_value + stats.reference_max_value);
        results.push_back(stats);
    }
    return results;
}

std::vector<FrameStatistics> BatchStatisticsEngine::compute(
    const beamlab::core::ISampleStorage& storage,
    const beamlab::data::AxisFrame& axisFrame,
    const FrameStatisticsParameters& parameters) const
{
    uint64_t total = storage.totalSampleCount();
    if (total < 3) return {};

    double sMin = std::numeric_limits<double>::infinity();
    double sMax = -std::numeric_limits<double>::infinity();

    constexpr uint64_t kScanBatch = 50000;
    uint64_t offset = 0;
    while (offset < total) {
        auto batch = storage.getBatch(offset, kScanBatch);
        for (const auto& sample : batch) {
            const auto relative = beamlab::core::subtract(sample.position_m, axisFrame.origin);
            const double s = beamlab::core::dot(relative, axisFrame.longitudinal);
            if (std::isfinite(s)) {
                sMin = std::min(sMin, s);
                sMax = std::max(sMax, s);
            }
        }
        offset += batch.size();
        if (batch.empty()) break;
    }

    if (!(sMax > sMin)) return {};

    auto binCount = static_cast<std::size_t>(parameters.axial_bin_count);
    if (binCount < 2) binCount = std::clamp(static_cast<std::size_t>(total / 500), std::size_t{24}, std::size_t{72});
    return computeUniformBins(storage, axisFrame, binCount, sMin, sMax);
}

} // namespace beamlab::analysis
