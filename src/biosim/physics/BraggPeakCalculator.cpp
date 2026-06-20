#include "biosim/physics/BraggPeakCalculator.h"
#include "biosim/physics/StoppingPowerEngine.h"

#include <algorithm>
#include <cmath>

namespace beamlab::biosim {

// ── Static helpers ────────────────────────────────────────────────────────────

double BraggPeakCalculator::kummerM(const double a, const double b, const double z)
{
    // ₁F₁(a;b;z) = Σ_{n≥0} (a)_n / (b)_n · z^n / n!   (A&S 13.1.2)
    // Ratio recurrence on the term avoids overflow of the rising factorials.
    double term = 1.0; // n = 0 term
    double sum  = 1.0;
    constexpr int k_max_terms = 500;
    for (int n = 0; n < k_max_terms; ++n) {
        term *= (a + static_cast<double>(n)) / (b + static_cast<double>(n))
                * z / static_cast<double>(n + 1);
        sum += term;
        if (std::abs(term) <= 1e-16 * std::abs(sum)) break;
    }
    return sum;
}

double BraggPeakCalculator::cylGauss(const double a, const double x)
{
    // g(a,x) = e^{−x²/4} · D_a(x), with D_a the parabolic cylinder function;
    // this product is the building block of the Bortfeld closed form (eq. 27).
    //
    // For very negative x the Kummer series loses accuracy, so use the standard
    // asymptotic D_a(x) ~ √(2π)/Γ(−a) · (−x)^(−a−1) · e^{x²/4} (A&S 19.8.1),
    // hence g(a,x) → √(2π)/Γ(−a) · (−x)^(−a−1).  Branch at x = −12 matches the
    // Bortfeld reference implementation (gray.mgh.harvard.edu BraggCurve.py).
    constexpr double k_branch   = -12.0;
    constexpr double k_sqrt_pi  = 1.7724538509055160273; // √π
    constexpr double k_sqrt_2pi = 2.5066282746310005024; // √(2π)

    if (x < k_branch) {
        return k_sqrt_2pi / std::tgamma(-a) * std::pow(-x, -a - 1.0);
    }

    // D_a(x) = 2^{a/2} e^{−x²/4} [ √π/Γ((1−a)/2) · M(−a/2, 1/2, x²/2)
    //                            − √(2π)·x/Γ(−a/2) · M((1−a)/2, 3/2, x²/2) ]
    //                                                            (A&S 19.3.1)
    // Folding in the leading e^{−x²/4} yields the e^{−x²/4} factor below.
    const double zz    = x * x / 2.0;
    const double term1 = k_sqrt_pi  / std::tgamma((1.0 - a) / 2.0)
                         * kummerM(-a / 2.0, 0.5, zz);
    const double term2 = k_sqrt_2pi * x / std::tgamma(-a / 2.0)
                         * kummerM((1.0 - a) / 2.0, 1.5, zz);
    return std::pow(2.0, a / 2.0) * std::exp(-zz) * (term1 - term2);
}

// ── CSDA Range ────────────────────────────────────────────────────────────────

double BraggPeakCalculator::csdaRange_cm(
    const double kinE_MeV,
    const BioMaterial& mat,
    const ParticleSpecies& particle,
    const int n_points) const
{
    if (kinE_MeV <= 0.0) return 0.0;

    // Lower limit: 0.01 MeV (below this Bethe-Bloch is unreliable)
    constexpr double E_min_MeV = 0.01;
    if (kinE_MeV <= E_min_MeV) return 0.0;

    // Number of Simpson intervals must be even
    const int N = (n_points % 2 == 0) ? n_points : n_points + 1;

    // Log-uniform energy grid from E_min to kinE_MeV
    const double log_Emin = std::log(E_min_MeV);
    const double log_Emax = std::log(kinE_MeV);
    const double dlog = (log_Emax - log_Emin) / static_cast<double>(N);

    StoppingPowerEngine engine;
    double integral = 0.0;

    // Composite Simpson's rule:  ∫f dx ≈ h/3 · (f0 + 4f1 + 2f2 + 4f3 + … + fN)
    // Variable substitution: E = exp(u), dE = E·du  → integrand = E / dEdx(E)
    for (int i = 0; i <= N; ++i) {
        const double u = log_Emin + static_cast<double>(i) * dlog;
        const double E = std::exp(u);
        const double dEdx = engine.dEdx_MeV_cm(E, mat, particle);
        if (dEdx <= 0.0) continue;
        const double f = E / dEdx; // integrand in log-space: 1/dEdx · dE/du = E/dEdx

        double coeff = 1.0;
        if (i == 0 || i == N) {
            coeff = 1.0;
        } else if (i % 2 == 1) {
            coeff = 4.0;
        } else {
            coeff = 2.0;
        }
        integral += coeff * f;
    }
    integral *= dlog / 3.0;

    return std::max(0.0, integral); // [cm]
}

double BraggPeakCalculator::bragPeakDepth_cm(
    const double kinE_MeV,
    const BioMaterial& mat,
    const ParticleSpecies& particle) const
{
    return csdaRange_cm(kinE_MeV, mat, particle);
}

// ── Bortfeld 1997 depth-dose model ───────────────────────────────────────────
//
// Reference: T. Bortfeld, "An analytical approximation of the Bragg curve for
// therapeutic proton beams", Med.Phys. 24(12):2024-2033, 1997.
//
// Closed form (eqs. 27-28), valid in the peak region z ≲ R₀ + 3σ:
//
//   D(z) = 0.65 · [ g(−1/p, ζ) + σ·k · g(−1/p−1, ζ) ],   ζ = (z − R₀)/σ
//
// where g(a,x) = e^{−x²/4}·D_a(x) is the Gaussian-weighted parabolic cylinder
// function (see cylGauss).  The first term is the range-straggled Bragg peak;
// the second is the nuclear-reaction "tail" (its weight k ∝ ε).
//
//   R₀ = CSDA range [cm]            p = 1.77   (Bragg-Kleeman exponent, water)
//   σ  = range-straggling width      k ≈ 0.01394 (nuclear tail factor)
//
// Unlike a bare power law, g(−1/p, ζ) has an integrable cusp at ζ→0⁻, which is
// what produces the sharp Bragg peak near z = R₀ (peak height ≫ entrance dose).

double BraggPeakCalculator::bortfeldDoseAtDepth(
    const double z_cm,
    const double R0_cm,
    const double sigma_cm,
    const double p,
    const double k) const
{
    if (sigma_cm <= 0.0) return 0.0;
    const double zeta = (z_cm - R0_cm) / sigma_cm;

    // Bortfeld 1997, eq. 27 (D100 factored out → returns shape, normalised later).
    constexpr double k_prefactor = 0.65; // eq. 27 leading constant
    return k_prefactor * (cylGauss(-1.0 / p, zeta)
                          + sigma_cm * k * cylGauss(-1.0 / p - 1.0, zeta));
}

std::vector<BraggPeakCalculator::BraggCurvePoint> BraggPeakCalculator::bortfeldProtonCurve(
    const double kinE_MeV,
    const BioMaterial& mat,
    const ParticleSpecies& particle,
    const int n_points,
    const double sigma_E_rel) const
{
    std::vector<BraggCurvePoint> curve;
    if (kinE_MeV <= 0.0 || n_points < 2) return curve;

    // CSDA range in the material
    const double R0_cm = csdaRange_cm(kinE_MeV, mat, particle);
    if (R0_cm <= 0.0) return curve;

    // Bortfeld parameters
    // p: energy-range (Bragg-Kleeman) exponent; for water p ≈ 1.77.  It is
    // approximately material-independent, so it is reused for other absorbers.
    constexpr double p = 1.77; // Bortfeld 1997, Fig. 1
    // k: nuclear-reaction tail factor (∝ ε, the low-energy fluence fraction).
    constexpr double k = 0.01394; // Bortfeld 1997, eq. 28 / reference fit

    // Range straggling sigma [cm] = σ_rel · √(range_in_cm)
    // Typical σ_rel ≈ 0.012 cm^(1/2) for protons in water; energy spread adds in quadrature.
    const double sigma_straggling = 0.012 * std::sqrt(R0_cm);  // Bohr straggling
    const double sigma_energy     = sigma_E_rel * R0_cm;        // from beam energy spread
    const double sigma_cm         = std::sqrt(sigma_straggling * sigma_straggling +
                                              sigma_energy * sigma_energy);

    // Evaluate D(z) at n_points depths from 0 to 1.1·R0
    const double z_max = 1.1 * R0_cm;
    const double dz    = z_max / static_cast<double>(n_points - 1);

    curve.reserve(static_cast<std::size_t>(n_points));
    double D_max = 0.0;

    for (int i = 0; i < n_points; ++i) {
        const double z = static_cast<double>(i) * dz;
        BraggCurvePoint pt;
        pt.depth_cm  = z;
        // Clamp to ≥ 0: the closed form can dip slightly negative in the far
        // tail (z ≫ R₀), but physical dose is non-negative.
        pt.dose_rel  = std::max(0.0, bortfeldDoseAtDepth(z, R0_cm, sigma_cm, p, k));
        D_max = std::max(D_max, pt.dose_rel);
        curve.push_back(pt);
    }

    // Normalise to maximum
    if (D_max > 0.0) {
        for (auto& pt : curve) {
            pt.dose_rel /= D_max;
        }
    }

    return curve;
}

} // namespace beamlab::biosim
