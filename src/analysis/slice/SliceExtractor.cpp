#include "analysis/slice/SliceExtractor.h"

#include "analysis/slice/SliceProjector.h"

namespace beamlab::analysis {

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

    const double s_min = curve_point.reference_min_value;
    const double s_max = curve_point.reference_max_value;

    const SliceProjector projector{};

    for (std::size_t ti = 0; ti < dataset.trajectories.size(); ++ti) {
        const auto& trajectory = dataset.trajectories[ti];

        for (std::size_t si = 0; si < trajectory.samples.size(); ++si) {
            const auto& sample = trajectory.samples[si];

            const double s = projector.projectToLongitudinalAxis(sample.position_m,
                                                                 dataset.axis_frame);

            if (s < s_min || s >= s_max) {
                continue;
            }

            beamlab::data::BeamSlicePoint point{};
            point.trajectory_index = ti;
            point.sample_index = si;
            point.position_m = sample.position_m;
            point.projected_uv_m = projector.projectToTransversePlane(sample.position_m,
                                                                      dataset.axis_frame);

            slice.points.push_back(point);
        }
    }

    return slice;
}

} // namespace beamlab::analysis
