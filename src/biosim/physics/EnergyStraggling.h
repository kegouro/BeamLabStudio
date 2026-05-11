#pragma once

#include "biosim/materials/BioMaterial.h"
#include "biosim/physics/ParticleLibrary.h"

#include <random>

namespace beamlab::biosim {

// Statistical energy-loss fluctuation models.
//
// The parameter κ = ξ / Wmax controls which distribution applies:
//   κ < 0.01   → Landau (thin absorber, few collisions)
//   0.01 ≤ κ < 10 → Vavilov (intermediate)
//   κ ≥ 10     → Gaussian (thick absorber, Central Limit Theorem)
//
// where:
//   ξ = (K · z² · Z/A · ρ · dx) / (2β²)   [MeV]
//   Wmax = max single-collision energy transfer [MeV]
//
// Reference:
//   Landau, JETP 8 (1944) 201
//   Vavilov, JETP 5 (1957) 749
//   Bohr straggling: Reviews of Modern Physics 40 (1968)
//   PDG 2022, section 34.2

enum class StragglingMode {
    Deterministic,  // return mean energy loss (no fluctuation)
    Landau,         // sample from Landau distribution (approximate with table)
    Vavilov,        // use Vavilov (falls back to Landau/Gauss at extremes)
};

struct StragglingResult {
    double energy_loss_MeV{0.0}; // sampled energy loss [MeV]
    double kappa{0.0};           // κ value (diagnostic)
    double xi_MeV{0.0};         // ξ [MeV] (scale of fluctuations)
    StragglingMode mode_used{StragglingMode::Deterministic};
};

class EnergyStraggling {
public:
    // Compute energy loss with statistical fluctuation (or mean in Deterministic mode).
    //
    // The caller passes a long-lived `rng` (typically one per track) so the
    // RNG state advances naturally between consecutive steps — no need to mix
    // step-dependent material into the seed.  This guarantees independent
    // samples within a track and reproducibility across runs when the rng is
    // seeded from BioSimConfig::rng_seed.
    [[nodiscard]] StragglingResult sample(
        double kinE_MeV,
        double dx_cm,
        const BioMaterial& mat,
        const ParticleSpecies& particle,
        StragglingMode mode,
        std::mt19937_64& rng) const;

    // Compute κ for the given step conditions.
    [[nodiscard]] double kappa(double kinE_MeV,
                                double dx_cm,
                                const BioMaterial& mat,
                                const ParticleSpecies& particle) const;

    // Bohr variance σ²_B [MeV²] for the given step.
    [[nodiscard]] double bohrVariance_MeV2(double kinE_MeV,
                                            double dx_cm,
                                            const BioMaterial& mat,
                                            const ParticleSpecies& particle) const;

private:
    // ξ = (K·z²·Z/A·ρ·dx) / (2β²) [MeV]
    double xi_MeV(double kinE_MeV,
                   double dx_cm,
                   const BioMaterial& mat,
                   const ParticleSpecies& particle) const;

    // Sample from a Landau distribution (Moyal approximation + tabulation).
    double sampleLandau(double xi, double mean_dEdx_dx, std::mt19937_64& rng) const;

    // Sample from a Gaussian distribution.
    double sampleGaussian(double mean, double sigma, std::mt19937_64& rng) const;

    // Wmax for the given kinematics
    static double wmax_MeV(double kinE_MeV, double mass_MeV);
};

} // namespace beamlab::biosim
