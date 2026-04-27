#pragma once

#include "core/math/Vec3.h"

namespace beamlab::core {

struct Plane3D {
    Vec3 origin{};
    Vec3 normal{};
};

} // namespace beamlab::core
