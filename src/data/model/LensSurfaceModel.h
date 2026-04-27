#pragma once

#include "geometry/model/Mesh.h"

#include <cstddef>
#include <string>

namespace beamlab::data {

struct LensSurfaceModel {
    std::string name{"effective_lens_surface"};
    std::string method_name{"lofted_envelopes"};

    std::size_t slice_count{0};
    std::size_t points_per_slice{0};

    double axial_min_m{0.0};
    double axial_max_m{0.0};

    beamlab::geometry::Mesh mesh{};

    bool valid{false};
};

} // namespace beamlab::data
