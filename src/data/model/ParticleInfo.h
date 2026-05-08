#pragma once

#include <string>

namespace beamlab::data {

// Particle-level metadata attached to a Trajectory.
// Populated by physics-aware importers (e.g. Geant4CsvImporter).
struct ParticleInfo {
    std::string particle_type{"unknown"};  // e.g. "mu-", "mu+", "e-", "proton"
    double charge{-1.0};                   // elementary charge units
    double mass_MeV{105.6583755};          // rest mass in MeV/c² (muon by default)
    double initial_kinE_MeV{0.0};          // kinetic energy at first recorded step
    int track_id{1};
    int event_id{0};
};

} // namespace beamlab::data
