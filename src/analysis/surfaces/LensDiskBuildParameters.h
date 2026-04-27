#pragma once

#include <cstddef>

namespace beamlab::analysis {

struct LensDiskBuildParameters {
    std::size_t boundary_point_count{128};
    std::size_t radial_layers{16};

    // Espesor físico de la lente efectiva.
    // El centro queda más grueso que el borde para generar una forma tipo lente biconvexa.
    double center_thickness_m{0.003};
    double edge_thickness_m{0.0005};
};

} // namespace beamlab::analysis
