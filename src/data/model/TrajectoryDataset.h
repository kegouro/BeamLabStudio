#pragma once

#include "data/ids/DatasetId.h"
#include "data/model/AxisFrame.h"
#include "data/model/DatasetMetadata.h"
#include "data/model/Trajectory.h"
#include "data/model/VariableRegistry.h"

#include <vector>

namespace beamlab::data {

struct TrajectoryDataset {
    DatasetId id{};
    DatasetMetadata metadata{};
    AxisFrame axis_frame{};
    VariableRegistry variables{};
    std::vector<Trajectory> trajectories{};
};

} // namespace beamlab::data
