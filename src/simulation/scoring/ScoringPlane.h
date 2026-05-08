#pragma once

#include <string>
#include <cstddef>

namespace beamlab::simulation {

// A virtual scoring plane perpendicular to the longitudinal beam axis.
// Can act as:
//   - ParticleCounter  : counts how many tracks cross this plane
//   - Entry detector   : registers the first crossing (beam entrance)
//   - Exit detector    : registers the last crossing (beam exit)
struct ScoringPlane {
    enum class Role {
        Counter,   // generic particle counter
        Entry,     // marks beam entry into a region
        Exit       // marks beam exit from a region
    };

    std::string id{};
    double axial_position_m{0.0};
    Role role{Role::Counter};
    bool enabled{true};

    // Filled by ScoringPlaneDetector or after simulation
    std::size_t crossing_count{0};
    double mean_kinE_MeV{0.0};      // mean kinetic energy at crossing
    double mean_edep_step_MeV{0.0}; // mean edep of the step that crosses this plane
};

} // namespace beamlab::simulation
