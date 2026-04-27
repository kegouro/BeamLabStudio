#pragma once

#include "core/math/Vec3.h"
#include "data/ids/SampleId.h"
#include "data/ids/TrajectoryId.h"
#include "data/model/SampleFlags.h"

namespace beamlab::data {

struct TrajectorySample {
    SampleId id{};
    TrajectoryId trajectory_id{};
    double time_s{0.0};
    beamlab::core::Vec3 position_m{};
    SampleFlags flags{SampleFlags::None};
};

} // namespace beamlab::data
