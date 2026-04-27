#pragma once

#include "data/model/AxisFrame.h"
#include "data/model/TrajectoryDataset.h"

namespace beamlab::io {

class AxisFrameResolver {
public:
    [[nodiscard]] beamlab::data::AxisFrame resolve(const beamlab::data::TrajectoryDataset& dataset) const;
};

} // namespace beamlab::io
