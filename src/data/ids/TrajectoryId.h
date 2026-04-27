#pragma once

#include "core/foundation/StrongId.h"

namespace beamlab::data {

struct TrajectoryIdTag {};
using TrajectoryId = beamlab::core::StrongId<TrajectoryIdTag>;

} // namespace beamlab::data
