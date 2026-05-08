#include "analysis/slice/SliceExtractor.h"

#include "analysis/slice/SliceProjector.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace beamlab::analysis {
namespace {

beamlab::core::Vec3 interpolate(const beamlab::core::Vec3& a,
                                const beamlab::core::Vec3& b,
                                const double t)
{
    return {
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t
    };
}

} // namespace

beamlab::data::BeamSlice SliceExtractor::extractFrameSlice(const beamlab::data::TrajectoryDataset& dataset,
                                                           const beamlab::data::FocusCurve& curve,
                                                           const std::size_t frame_index) const
{
    beamlab::data::BeamSlice slice{};
    slice.slice_index = frame_index;

    if (frame_index >= curve.points.size()) {
        return slice;
    }

    const auto& curve_point = curve.points[frame_index];

    slice.reference_time_s = curve_point.reference_value;
    slice.axial_position_m = curve_point.reference_value;

    const double target_s = curve_point.reference_value;
    const double s_min = curve_point.reference_min_value;
    const double s_max = curve_point.reference_max_value;

    const SliceProjector projector{};

    for (std::size_t ti = 0; ti < dataset.trajectories.size(); ++ti) {
        const auto& trajectory = dataset.trajectories[ti];

        if (trajectory.samples.empty()) {
            continue;
        }

        bool added_intersection = false;

        for (std::size_t si = 1; si < trajectory.samples.size(); ++si) {
            const auto& previous = trajectory.samples[si - 1];
            const auto& current = trajectory.samples[si];

            const double s0 = projector.projectToLongitudinalAxis(previous.position_m,
                                                                  dataset.axis_frame);
            const double s1 = projector.projectToLongitudinalAxis(current.position_m,
                                                                  dataset.axis_frame);

            if (!std::isfinite(s0) || !std::isfinite(s1)) {
                continue;
            }

            const double lower = std::min(s0, s1);
            const double upper = std::max(s0, s1);

            if (target_s < lower || target_s > upper) {
                continue;
            }

            const double ds = s1 - s0;
            const double t = std::abs(ds) > std::numeric_limits<double>::epsilon()
                ? std::clamp((target_s - s0) / ds, 0.0, 1.0)
                : 0.0;
            const auto position = interpolate(previous.position_m, current.position_m, t);

            beamlab::data::BeamSlicePoint point{};
            point.trajectory_index = ti;
            point.sample_index = si - 1;
            point.position_m = position;
            point.projected_uv_m = projector.projectToTransversePlane(position,
                                                                      dataset.axis_frame);

            slice.points.push_back(point);
            added_intersection = true;
            break;
        }

        if (added_intersection) {
            continue;
        }

        double best_distance = std::numeric_limits<double>::infinity();
        const beamlab::data::TrajectorySample* best_sample = nullptr;
        std::size_t best_sample_index = 0;

        for (std::size_t si = 0; si < trajectory.samples.size(); ++si) {
            const auto& sample = trajectory.samples[si];
            const double s = projector.projectToLongitudinalAxis(sample.position_m,
                                                                 dataset.axis_frame);

            if (s < s_min || s > s_max) {
                continue;
            }

            const double distance = std::abs(s - target_s);
            if (distance < best_distance) {
                best_distance = distance;
                best_sample = &sample;
                best_sample_index = si;
            }
        }

        if (best_sample != nullptr) {
            beamlab::data::BeamSlicePoint point{};
            point.trajectory_index = ti;
            point.sample_index = best_sample_index;
            point.position_m = best_sample->position_m;
            point.projected_uv_m = projector.projectToTransversePlane(best_sample->position_m,
                                                                      dataset.axis_frame);

            slice.points.push_back(point);
        }
    }

    return slice;
}

} // namespace beamlab::analysis
