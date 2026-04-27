#pragma once

#include "core/math/Vec3.h"

namespace beamlab::core {

struct AABB {
    Vec3 min{};
    Vec3 max{};
};

} // namespace beamlab::core
