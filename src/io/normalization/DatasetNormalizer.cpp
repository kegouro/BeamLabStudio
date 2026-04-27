#include "io/normalization/DatasetNormalizer.h"

#include "io/normalization/AxisFrameResolver.h"
#include "io/normalization/EvolutionAxisResolver.h"

#include <algorithm>

namespace beamlab::io {

beamlab::data::TrajectoryDataset DatasetNormalizer::normalize(beamlab::data::TrajectoryDataset dataset) const
{
    for (auto& trajectory : dataset.trajectories) {
        std::sort(trajectory.samples.begin(), trajectory.samples.end(),
                  [](const auto& a, const auto& b) {
                      return a.time_s < b.time_s;
                  });

        trajectory.summary.sample_count = trajectory.samples.size();

        if (!trajectory.samples.empty()) {
            trajectory.summary.time_min_s = trajectory.samples.front().time_s;
            trajectory.summary.time_max_s = trajectory.samples.back().time_s;

            trajectory.summary.bounds.min = trajectory.samples.front().position_m;
            trajectory.summary.bounds.max = trajectory.samples.front().position_m;

            for (const auto& sample : trajectory.samples) {
                trajectory.summary.bounds.min.x = std::min(trajectory.summary.bounds.min.x, sample.position_m.x);
                trajectory.summary.bounds.min.y = std::min(trajectory.summary.bounds.min.y, sample.position_m.y);
                trajectory.summary.bounds.min.z = std::min(trajectory.summary.bounds.min.z, sample.position_m.z);

                trajectory.summary.bounds.max.x = std::max(trajectory.summary.bounds.max.x, sample.position_m.x);
                trajectory.summary.bounds.max.y = std::max(trajectory.summary.bounds.max.y, sample.position_m.y);
                trajectory.summary.bounds.max.z = std::max(trajectory.summary.bounds.max.z, sample.position_m.z);
            }
        }
    }

    const EvolutionAxisResolver evolution_resolver{};
    const auto evolution_axis = evolution_resolver.resolve(dataset);
    (void)evolution_axis;

    const AxisFrameResolver axis_frame_resolver{};
    dataset.axis_frame = axis_frame_resolver.resolve(dataset);

    return dataset;
}

} // namespace beamlab::io
