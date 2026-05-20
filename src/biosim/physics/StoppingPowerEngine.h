#pragma once

#include "biosim/materials/BioMaterial.h"
#include "biosim/physics/ParticleLibrary.h"

namespace beamlab::biosim {

// Full stopping-power calculations for charged heavy particles in matter.
//
// Includes:
//   - Bethe-Bloch mean stopping power with Sternheimer density correction
//   - Energy loss over a finite step (clamped to KinE)
//   - Multiple Coulomb Scattering deflection angle (Highland formula)
//   - Dose per step
//
// Reference:
//   Bethe-Bloch: PDG 2022, Chapter 34
//   Sternheimer: Sternheimer, Berger & Seltzer, At.Data Nucl.Data Tables 30 (1984)
//   Highland MCS: Highland 1975, Nucl.Instr.Meth. 129:497; Lynch & Dahl 1991
class StoppingPowerEngine {
public:
    // ── Stopping power ────────────────────────────────────────────────────────

    // Mean linear stopping power -dE/dx [MeV/cm].
    // Includes Sternheimer density correction when mat.has_sternheimer = true.
    [[nodiscard]] double dEdx_MeV_cm(double kinE_MeV,
                                      const BioMaterial& mat,
                                      const ParticleSpecies& particle) const;

    // Mass stopping power -dE/(ρ·dx) [MeV·cm²/g].
    [[nodiscard]] double massStoppingPower(double kinE_MeV,
                                            const BioMaterial& mat,
                                            const ParticleSpecies& particle) const;

    // Energy lost traversing dx_cm of material.
    // Returns [MeV], clamped so the particle cannot lose more than its kinetic energy.
    [[nodiscard]] double energyLoss_MeV(double kinE_MeV,
                                         double dx_cm,
                                         const BioMaterial& mat,
                                         const ParticleSpecies& particle) const;

    // ── Dose ─────────────────────────────────────────────────────────────────

    // Dose deposited [Gy] in a cylindrical voxel of radius r_cm and length dx_cm.
    // Uses D = edep [J] / mass [kg].
    [[nodiscard]] static double dose_Gy(double edep_MeV,
                                         const BioMaterial& mat,
                                         double dx_cm,
                                         double r_cm = 0.5642);

    // ── Multiple Coulomb Scattering (Highland formula) ────────────────────────

    // RMS scattering angle θ₀ [radians] after traversing dx_cm.
    // θ₀ = (13.6 MeV / (β·c·p)) · |z| · √(dx/X₀) · [1 + 0.038·ln(dx/X₀)]
    //
    // Returns 0 if X₀ is not available (radiation_length_cm ≤ 0).
    [[nodiscard]] double mcsAngle_rad(double kinE_MeV,
                                       double dx_cm,
                                       const BioMaterial& mat,
                                       const ParticleSpecies& particle) const;

    // Projected lateral displacement RMS [cm] due to MCS over dx_cm.
    // y_rms = dx / √3 · θ₀
    [[nodiscard]] double mcsLateralDisplacement_cm(double kinE_MeV,
                                                    double dx_cm,
                                                    const BioMaterial& mat,
                                                    const ParticleSpecies& particle) const;
};

} // namespace beamlab::biosim
