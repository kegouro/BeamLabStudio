#pragma once

#include "core/math/Vec3.h"

#include <string>

namespace beamlab::data {

struct AxisFrame {
    beamlab::core::Vec3 origin{};
    beamlab::core::Vec3 longitudinal{};
    beamlab::core::Vec3 transverse_u{};
    beamlab::core::Vec3 transverse_v{};
    std::string derivation_method{"undefined"};
    double confidence{0.0};
};

} // namespace beamlab::data
