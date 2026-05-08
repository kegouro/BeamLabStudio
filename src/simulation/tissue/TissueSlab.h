#pragma once

#include "core/math/Vec3.h"
#include "simulation/tissue/TissueMaterial.h"

#include <string>

namespace beamlab::simulation {

// A finite slab of biological material positioned in the beam coordinate system.
// The slab is a plane perpendicular to the longitudinal axis, with a given
// thickness along that axis.
struct TissueSlab {
    std::string id{};               // unique user-defined label
    TissueMaterial material{};
    double axial_start_m{0.0};      // start position along the longitudinal axis (m)
    double thickness_m{0.01};       // slab thickness in meters
    bool enabled{true};

    // Derived: end position
    [[nodiscard]] double axialEnd() const { return axial_start_m + thickness_m; }
};

} // namespace beamlab::simulation
