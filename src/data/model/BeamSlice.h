#pragma once

#include "core/math/Vec2.h"
#include "core/math/Vec3.h"

#include <cstddef>
#include <vector>

namespace beamlab::data {

struct BeamSlicePoint {
    std::size_t trajectory_index{0};
    std::size_t sample_index{0};

    beamlab::core::Vec3 position_m{};
    beamlab::core::Vec2 projected_uv_m{};
};

struct BeamSlice {
    std::size_t slice_index{0};
    double reference_time_s{0.0};
    double axial_position_m{0.0};

    std::vector<BeamSlicePoint> points{};
};

} // namespace beamlab::data
