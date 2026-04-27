#pragma once

#include "data/ids/TrajectoryId.h"
#include "data/model/TrajectorySample.h"
#include "data/model/TrajectorySummary.h"

#include <vector>

namespace beamlab::data {

struct Trajectory {
    TrajectoryId id{};
    std::vector<TrajectorySample> samples{};
    TrajectorySummary summary{};
};

} // namespace beamlab::data
