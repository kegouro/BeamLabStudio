#pragma once

#include "biosim/core/EnergyScaleSet.h"
#include "biosim/materials/BioMaterial.h"
#include "biosim/physics/ParticleLibrary.h"

namespace beamlab::biosim {

// Physical constants used in all conversions.
namespace PhysConst {
    constexpr double MeV_to_J      = 1.602176634e-13;  // exact (2019 SI)
    constexpr double J_to_erg      = 1.0e7;
    constexpr double Gy_to_rad     = 100.0;
    constexpr double Sv_to_rem     = 100.0;
    constexpr double m_e_MeV       = 0.51099895;        // electron rest mass [MeV/c²]
    constexpr double k_BB          = 0.307075;          // Bethe-Bloch K [MeV·cm²/mol]
}

// Converts a (edep_MeV, kinE_MeV, step geometry, material, particle) tuple
// into a fully populated EnergyScaleSet.
//
// Design rule: every field in EnergyScaleSet is computed from the primary
// edep_MeV_simulated value using exact arithmetic (no chained rounding).
class EnergyScaleConverter {
public:
    // Compute the complete EnergyScaleSet for one step.
    //
    // Parameters:
    //   edep_MeV_orig    : Geant4 original energy deposit [MeV]
    //   edep_MeV_sim     : biosim recalculated energy deposit [MeV]
    //                      (equals edep_MeV_orig outside active slabs)
    //   kinE_MeV_orig    : Geant4 original kinetic energy [MeV]
    //   kinE_MeV_sim     : biosim kinetic energy after propagation [MeV]
    //   step_length_cm   : Euclidean step length [cm]
    //   depth_in_slab_cm : depth inside the active slab [cm]; 0 if outside
    //   mat              : material of the active slab (nullptr → vacuum/no slab)
    //   particle         : particle species
    //   slab_index       : index of the active slab; -1 if none
    //   beam_radius_cm   : effective beam radius for dose calculation [cm]
    //                      use 0.5642 cm to get 1 cm² cross-section (r = √(1/π))
    [[nodiscard]] EnergyScaleSet compute(
        double edep_MeV_orig,
        double edep_MeV_sim,
        double kinE_MeV_orig,
        double kinE_MeV_sim,
        double step_length_cm,
        double depth_in_slab_cm,
        const BioMaterial* mat,
        const ParticleSpecies& particle,
        int slab_index,
        double beam_radius_cm = 0.5642) const;

    // Compute LET (-dE/dx) at the given kinetic energy and material.
    // Returns MeV/cm (linear stopping power).
    [[nodiscard]] double computeLET_MeV_cm(double kinE_MeV,
                                            const BioMaterial& mat,
                                            const ParticleSpecies& particle) const;

    // CSDA range estimate: R ≈ KinE / <dE/dx>  (crude single-point approximation).
    // For an accurate value, use BraggPeakCalculator::csdaRange().
    [[nodiscard]] double estimateCsdaRange_cm(double kinE_MeV,
                                               const BioMaterial& mat,
                                               const ParticleSpecies& particle) const;

    // RBE estimate from LET using the Paganetti 2014 parameterization for protons.
    // For other particles WR is used directly (conservative).
    [[nodiscard]] double estimateRBE(double LET_keV_um,
                                      const BioMaterial& mat,
                                      const ParticleSpecies& particle) const;
};

} // namespace beamlab::biosim
