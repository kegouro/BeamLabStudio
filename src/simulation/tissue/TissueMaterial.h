#pragma once

#include <string>

namespace beamlab::simulation {

// Physical properties of a biological material needed for stopping-power calculations.
struct TissueMaterial {
    std::string name{};          // e.g. "Water", "Muscle", "Cortical Bone"
    std::string symbol{};        // short id used in config, e.g. "H2O", "muscle"
    double density_g_cm3{1.0};   // mass density in g/cm³
    double Z_eff{7.22};          // effective atomic number
    double A_eff{12.01};         // effective atomic mass in g/mol
    double I_eV{75.0};           // mean excitation energy in eV (ICRU values)
    double radiation_length_cm{36.08}; // X0 in cm (for reference/future use)
};

} // namespace beamlab::simulation
