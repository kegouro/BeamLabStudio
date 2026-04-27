#pragma once

#include "core/math/Vec2.h"

#include <cstddef>
#include <string>
#include <vector>

namespace beamlab::data {

struct BeamEnvelope {
    std::size_t slice_index{0};
    double reference_time_s{0.0};
    double axial_position_m{0.0};

    std::string method_name{};
    std::size_t input_point_count{0};
    std::vector<beamlab::core::Vec2> boundary_points{};

    double area_m2{0.0};
    double perimeter_m{0.0};
    bool valid{false};
};

} // namespace beamlab::data
