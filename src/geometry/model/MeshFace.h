#pragma once

#include <cstddef>

namespace beamlab::geometry {

struct MeshFace {
    std::size_t a{0};
    std::size_t b{0};
    std::size_t c{0};
};

} // namespace beamlab::geometry
