#pragma once

#include "biosim/materials/BioMaterial.h"
#include "biosim/physics/ParticleLibrary.h"

#include <vector>

namespace beamlab::biosim {

// Bragg peak depth-dose profile calculation.
//
// Two methods are provided:
//   1. CSDA Range — numerical integration of 1/(-dE/dx) over kinetic energy.
//   2. Bortfeld analytical model — approximate depth-dose for a mono-energetic
//      proton beam in a homogeneous absorber (Bortfeld 1997, Med.Phys. 24:2024).
//
// For heavy ions (C-12, O-16, etc.) the Bortfeld model is applied with
// appropriate scaling but results are approximate; Monte Carlo (Geant4) is
// the authoritative source.
class BraggPeakCalculator {
public:
    // ── CSDA Range ────────────────────────────────────────────────────────────

    // Compute the Continuous Slowing Down Approximation (CSDA) range [cm]
    // for a particle with initial kinetic energy kinE_MeV in the given material.
    //
    // Algorithm:
    //   Integrate R = ∫[0→T₀] dT / |dE/dx(T)|
    //   using composite Simpson's rule with n_points (must be even, default 1000)
    //   on a log-uniform energy grid from E_min to T₀.
    [[nodiscard]] double csdaRange_cm(double kinE_MeV,
                                       const BioMaterial& mat,
                                       const ParticleSpecies& particle,
                                       int n_points = 1000) const;

    // Depth at which the particle stops [cm] (same as csdaRange_cm for CSDA).
    [[nodiscard]] double bragPeakDepth_cm(double kinE_MeV,
                                           const BioMaterial& mat,
                                           const ParticleSpecies& particle) const;

    // ── Bortfeld depth-dose model ─────────────────────────────────────────────

    struct BraggCurvePoint {
        double depth_cm{0.0};   // depth in material [cm]
        double dose_rel{0.0};   // relative dose D(z)/D_max [0..1]
    };

    // Compute the Bortfeld 1997 depth-dose curve.
    //
    // IMPORTANT: this model is calibrated for **protons** in water (Bortfeld
    // 1997, Med.Phys. 24:2024).  Applying it to muons or heavy ions reuses
    // the same parametric form with a different CSDA range, so the qualitative
    // shape is preserved but the Bragg peak depth/height has a systematic
    // bias (≈20-30 % for muons because their dE/dx vs. depth is much flatter
    // than protons').  The renamed entry point makes the calibration domain
    // explicit at every call site.
    //
    // Parameters:
    //   kinE_MeV    : initial beam kinetic energy [MeV]
    //   mat         : absorber material
    //   particle    : beam particle (optimised for protons; approximate for others)
    //   n_points    : number of depth points to evaluate
    //   sigma_E_rel : relative energy spread σ_E / E₀ (beam emittance, default 0.01 = 1%)
    //
    // Returns a vector of (depth_cm, dose_rel) pairs sorted by depth_cm.
    // dose_rel is normalised so that max(dose_rel) = 1.0.
    [[nodiscard]] std::vector<BraggCurvePoint> bortfeldProtonCurve(
        double kinE_MeV,
        const BioMaterial& mat,
        const ParticleSpecies& particle,
        int n_points = 300,
        double sigma_E_rel = 0.01) const;

private:
    // Evaluate the Bortfeld 1997 closed-form depth-dose D(z) (Med.Phys. 24:2024,
    // eqs. 27-28) at depth z_cm.  Returns unnormalised dose [a.u.].
    //   R0_cm    : CSDA range [cm]
    //   sigma_cm : range-straggling width σ [cm]
    //   p        : Bragg-Kleeman exponent (1.77 for water)
    //   k        : nuclear-reaction tail scaling factor (∝ ε, ≈ 0.01394)
    double bortfeldDoseAtDepth(double z_cm,
                                double R0_cm,
                                double sigma_cm,
                                double p,
                                double k) const;

    // Gaussian-weighted parabolic cylinder function  g(a,x) = e^{−x²/4}·D_a(x),
    // the building block of the Bortfeld closed form (Bortfeld 1997, eq. 27).
    // For x < −12 uses the asymptotic √(2π)/Γ(−a)·(−x)^(−a−1); otherwise
    // evaluates D_a(x) via Kummer's confluent hypergeometric function.
    static double cylGauss(double a, double x);

    // Kummer confluent hypergeometric function ₁F₁(a;b;z) via its power series
    // (Abramowitz & Stegun 13.1.2).
    static double kummerM(double a, double b, double z);
};

} // namespace beamlab::biosim
