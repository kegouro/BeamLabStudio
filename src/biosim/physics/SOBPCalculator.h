#pragma once

// Spread-Out Bragg Peak (SOBP) calculator for proton therapy.
//
// A SOBP is formed by superimposing N mono-energetic proton Bragg curves
// with analytically derived weights so that the summed depth-dose is flat
// (within clinical tolerance) over a user-specified proximal–distal plateau.
//
// Weight algorithm — Gaussian-elimination solution of A·w = 1
//   (Bortfeld & Schlegel 1996, Phys. Med. Biol. 41:1331, eq. (8)):
//
//   Build the N×N dose matrix  A[k][j] = D_j(z_k), where z_k is the k-th
//   uniformly-spaced sample depth in [proximal, distal] and D_j is the
//   normalised Bortfeld curve for energy j.  Solve the linear system A·w = 1
//   by Gaussian elimination with partial pivoting.  This forces
//     SOBP(z_k) = Σ_j w_j · D_j(z_k) = 1  for every k.
//
//   No quadratic optimiser is required.  The O(N³) solve is negligible for
//   N ≤ 30 (the clinical range; ~27 µs on modern hardware).
//
// Physics:
//   Material : liquid water (ICRU-44, id = "water_icru")
//   Particle : proton (id = "proton")
//   Depth-dose: Bortfeld 1997 analytical model (BraggPeakCalculator::bortfeldProtonCurve)
//   Range     : CSDA (BraggPeakCalculator::csdaRange_cm)

#include "biosim/materials/BioMaterialLibrary.h"
#include "biosim/physics/BraggPeakCalculator.h"
#include "biosim/physics/ParticleLibrary.h"

#include <vector>

namespace beamlab::biosim {

// A single-energy component of an SOBP.
struct SOBPComponent {
    double energy_MeV{0.0};  // proton kinetic energy [MeV]
    double weight{0.0};      // relative weight w_k [0..1]
    double range_cm{0.0};    // CSDA range in water [cm]
    // Resampled dose curve on the shared depth grid.
    // dose_grid[i] = weight * normalised Bortfeld dose at depth_grid[i]
    std::vector<double> weighted_dose{};
};

// Result of an SOBP computation.
struct SOBPResult {
    bool valid{false};
    std::string error{};

    // Shared depth grid [cm] — same length as sobp_dose and each component's
    // weighted_dose vector.
    std::vector<double> depth_grid_cm{};

    // SOBP(z) = Σ w_k * D_k(z), normalised to max = 1.
    std::vector<double> sobp_dose{};

    // Individual components (for rendering individual Bragg curves).
    std::vector<SOBPComponent> components{};

    // Achieved plateau flatness: max deviation from 1.0 in [proximal, distal].
    // A value ≤ 0.10 means the plateau is flat within ±10%.
    double plateau_max_deviation{0.0};

    // Target plateau bounds used during computation.
    double proximal_cm{0.0};
    double distal_cm{0.0};
};

// Parameters for an SOBP calculation.
struct SOBPParams {
    // Maximum proton energy [MeV] — sets the deepest Bragg peak.
    // Clinical proton therapy: 70–250 MeV → range ~4–33 cm in water.
    double energy_max_MeV{150.0};

    // Proximal depth of the target plateau [cm].
    double proximal_cm{10.0};

    // Distal depth of the target plateau [cm] (= range of deepest peak).
    double distal_cm{15.0};

    // Number of mono-energetic peaks to superpose (default 10).
    // More peaks → flatter SOBP; 8–15 is typical in clinic.
    int n_peaks{10};

    // Depth resolution [cm] for the output grid.
    double depth_step_cm{0.02};

    // Number of depth points per individual Bortfeld curve.
    int curve_n_points{300};

    // Relative energy spread σ_E / E₀ (beam emittance).
    double sigma_E_rel{0.01};
};

// Pure-C++ SOBP calculator (no Qt dependency — testable headless).
class SOBPCalculator {
public:
    SOBPCalculator();

    // Compute an SOBP for the given parameters.
    // Returns SOBPResult::valid = false with an error message on failure.
    [[nodiscard]] SOBPResult compute(const SOBPParams& params) const;

    // Invert range → energy: find E such that csdaRange_cm(E) ≈ target_cm.
    // Uses bisection on [E_lo, E_hi] (default 1–300 MeV) with tolerance tol_cm.
    // Returns 0.0 if no solution found.
    [[nodiscard]] double energyForRange_cm(double target_cm,
                                           double E_lo_MeV = 1.0,
                                           double E_hi_MeV = 300.0,
                                           double tol_cm   = 1e-4) const;

private:
    BioMaterial   water_;
    ParticleSpecies proton_;
    BraggPeakCalculator calc_;

    // Resample a Bortfeld curve onto a uniform depth grid.
    // Uses linear interpolation; extrapolates 0.0 beyond curve bounds.
    static std::vector<double> resampleCurve(
        const std::vector<BraggPeakCalculator::BraggCurvePoint>& curve,
        const std::vector<double>& grid);
};

} // namespace beamlab::biosim
