#include "io/normalization/AxisFrameResolver.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>

namespace beamlab::io {
namespace {

struct AxisStats {
    double min_x{std::numeric_limits<double>::infinity()};
    double min_y{std::numeric_limits<double>::infinity()};
    double min_z{std::numeric_limits<double>::infinity()};

    double max_x{-std::numeric_limits<double>::infinity()};
    double max_y{-std::numeric_limits<double>::infinity()};
    double max_z{-std::numeric_limits<double>::infinity()};

    double signed_dx_sum{0.0};
    double signed_dy_sum{0.0};
    double signed_dz_sum{0.0};

    std::size_t segment_count{0};
    bool valid{false};
};

AxisStats computeStats(const beamlab::data::TrajectoryDataset& dataset)
{
    AxisStats stats{};

    for (const auto& trajectory : dataset.trajectories) {
        for (const auto& sample : trajectory.samples) {
            const auto& p = sample.position_m;

            stats.min_x = std::min(stats.min_x, p.x);
            stats.min_y = std::min(stats.min_y, p.y);
            stats.min_z = std::min(stats.min_z, p.z);

            stats.max_x = std::max(stats.max_x, p.x);
            stats.max_y = std::max(stats.max_y, p.y);
            stats.max_z = std::max(stats.max_z, p.z);

            stats.valid = true;
        }

        if (trajectory.samples.size() >= 2) {
            const auto& first = trajectory.samples.front().position_m;
            const auto& last = trajectory.samples.back().position_m;

            stats.signed_dx_sum += last.x - first.x;
            stats.signed_dy_sum += last.y - first.y;
            stats.signed_dz_sum += last.z - first.z;
            ++stats.segment_count;
        }
    }

    return stats;
}

double signFromSum(const double value)
{
    return value < 0.0 ? -1.0 : 1.0;
}

} // namespace

beamlab::data::AxisFrame AxisFrameResolver::resolve(const beamlab::data::TrajectoryDataset& dataset) const
{
    const auto stats = computeStats(dataset);

    beamlab::data::AxisFrame frame{};
    frame.origin = {0.0, 0.0, 0.0};

    if (!stats.valid) {
        frame.longitudinal = {0.0, 1.0, 0.0};
        frame.transverse_u = {1.0, 0.0, 0.0};
        frame.transverse_v = {0.0, 0.0, 1.0};
        frame.derivation_method = "fallback_default_y_axis";
        frame.confidence = 0.05;
        return frame;
    }

    const double range_x = stats.max_x - stats.min_x;
    const double range_y = stats.max_y - stats.min_y;
    const double range_z = stats.max_z - stats.min_z;

    if (range_z >= range_x && range_z >= range_y) {
        const double s = signFromSum(stats.signed_dz_sum);
        frame.longitudinal = {0.0, 0.0, s};
        frame.transverse_u = {1.0, 0.0, 0.0};
        frame.transverse_v = {0.0, 1.0, 0.0};
        frame.derivation_method = "auto_largest_extent_z_axis";
    } else if (range_y >= range_x && range_y >= range_z) {
        const double s = signFromSum(stats.signed_dy_sum);
        frame.longitudinal = {0.0, s, 0.0};
        frame.transverse_u = {1.0, 0.0, 0.0};
        frame.transverse_v = {0.0, 0.0, 1.0};
        frame.derivation_method = "auto_largest_extent_y_axis";
    } else {
        const double s = signFromSum(stats.signed_dx_sum);
        frame.longitudinal = {s, 0.0, 0.0};
        frame.transverse_u = {0.0, 1.0, 0.0};
        frame.transverse_v = {0.0, 0.0, 1.0};
        frame.derivation_method = "auto_largest_extent_x_axis";
    }

    const double largest = std::max({range_x, range_y, range_z});
    const double total = range_x + range_y + range_z;
    frame.confidence = total > 0.0 ? largest / total : 0.05;

    return frame;
}

} // namespace beamlab::io
