#pragma once

#include "simulation/tissue/TissueMaterial.h"

namespace beamlab::simulation {

// Computes the Bethe-Bloch mean energy loss (-dE/dx) for a charged particle
// traversing a material.  Units: MeV / cm.
//
// Reference: PDG Review, Chapter 34 "Passage of Particles Through Matter".
// Valid for β·γ > 0.1 (well above threshold for δ-ray production).
class BetheBlochEngine {
public:
    // Compute -dE/dx in MeV/cm for a particle with:
    //   kinE_MeV     : kinetic energy in MeV
    //   mass_MeV     : rest mass in MeV/c²  (muon = 105.66)
    //   charge       : particle charge in units of |e|  (muon = ±1)
    //   material     : target material
    [[nodiscard]] double dEdx_MeV_cm(double kinE_MeV,
                                      double mass_MeV,
                                      double charge,
                                      const TissueMaterial& material) const;

    // Energy lost traversing a thickness of `dx_cm` cm.
    // Returns energy loss in MeV (positive value).
    // Clamps so that the particle cannot lose more than its kinetic energy.
    [[nodiscard]] double energyLoss_MeV(double kinE_MeV,
                                         double mass_MeV,
                                         double charge,
                                         const TissueMaterial& material,
                                         double dx_cm) const;

    // Dose deposited (Gy = J/kg) in a voxel of the given material
    // when the particle deposits edep_MeV in it.
    // Requires the voxel mass in kg.
    [[nodiscard]] static double dose_Gy(double edep_MeV, double mass_kg);

    // Dose deposited (Gy) given the energy lost in a path of dx_cm in `material`.
    // Assumes the energy is deposited uniformly in a cylinder of radius `r_cm`
    // along the step. If r_cm is 0, uses the linear stopping-power model directly.
    [[nodiscard]] double dosePerStep_Gy(double edep_MeV,
                                         const TissueMaterial& material,
                                         double dx_cm) const;
};

} // namespace beamlab::simulation
