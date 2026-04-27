#pragma once

#include "data/model/TrajectoryDataset.h"

#include <string>

namespace beamlab::io {

class EvolutionAxisResolver {
public:
    [[nodiscard]] std::string resolve(const beamlab::data::TrajectoryDataset& dataset) const;
};

} // namespace beamlab::io
