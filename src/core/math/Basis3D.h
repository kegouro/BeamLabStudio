#pragma once

#include "core/math/Vec3.h"

namespace beamlab::core {

struct Basis3D {
    Vec3 e1{};
    Vec3 e2{};
    Vec3 e3{};
};

} // namespace beamlab::core
