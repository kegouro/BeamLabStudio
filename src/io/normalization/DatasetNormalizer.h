#pragma once

#include "data/model/TrajectoryDataset.h"

namespace beamlab::io {

class DatasetNormalizer {
public:
    [[nodiscard]] beamlab::data::TrajectoryDataset normalize(beamlab::data::TrajectoryDataset dataset) const;
};

} // namespace beamlab::io
