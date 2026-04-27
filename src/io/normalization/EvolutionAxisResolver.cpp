#include "io/normalization/EvolutionAxisResolver.h"

namespace beamlab::io {

std::string EvolutionAxisResolver::resolve(const beamlab::data::TrajectoryDataset& dataset) const
{
    for (const auto& trajectory : dataset.trajectories) {
        if (!trajectory.samples.empty()) {
            return "time_s";
        }
    }

    return "undefined";
}

} // namespace beamlab::io
