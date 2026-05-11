#include "biosim/physics/BraggPeakCalculator.h"
#include "biosim/physics/StoppingPowerEngine.h"

#include <algorithm>
#include <cmath>

namespace beamlab::biosim {

// ── Static helpers ────────────────────────────────────────────────────────────

double BraggPeakCalculator::normalCDF(const double x)
{
    // φ(x) = 0.5 · erfc(−x / √2)
    return 0.5 * std::erfc(-x / std::sqrt(2.0));
}

double BraggPeakCalculator::betaFunction(const double a, const double b)
{
    // B(a,b) = exp(lgamma(a) + lgamma(b) - lgamma(a+b))
    return std::exp(std::lgamma(a) + std::lgamma(b) - std::lgamma(a + b));
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
// Reference: Bortfeld, Med.Phys. 24(12):2024, 1997.
//
// The model represents the depth-dose of a proton pencil beam as:
//
//   D(z) ∝ Φ₀ · (1 + 0.012·R₀) ·
//           [ 0.99 · (R₀ − z)^(p−1) / B(p, 0.012·R₀+1)   ... spread peak
//           + 0.00264 · R₀^p · φ((z − R₀·(1−σ²·p/R₀)) / (σ·√R₀)) ] ... Gaussian tail
//
// where:
//   R₀    = CSDA range [cm]
//   p     = 1.77 (empirical energy-range exponent for water)
//   σ     = 0.012 cm^(1/2)  (range straggling parameter)
//   φ     = Gaussian CDF
//   B     = Beta function
//   Φ₀   = primary fluence (normalised to 1)

double BraggPeakCalculator::bortfeldDoseAtDepth(
    const double z_cm,
    const double R0_cm,
    const double sigma_cm,  // σ·√R₀  [cm]
    const double p) const
{
    const double diff = R0_cm - z_cm;

    // Peak term: only for z < R0
    double peak_term = 0.0;
    if (diff > 0.0) {
        const double bfunc = betaFunction(p, 0.012 * R0_cm + 1.0);
        if (bfunc > 0.0) {
            peak_term = 0.99 * std::pow(diff, p - 1.0) / bfunc;
        }
    }

    // Gaussian tail term (accounts for range straggling and secondary electrons)
    const double mu_gauss = R0_cm - sigma_cm * sigma_cm * p / R0_cm;
    const double tail_term = 0.00264 * std::pow(R0_cm, p) *
                              normalCDF((z_cm - mu_gauss) / sigma_cm);

    return (1.0 + 0.012 * R0_cm) * (peak_term + tail_term);
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
    // p: energy-range exponent; for water p ≈ 1.77. Scale for other materials
    // using the density correction: p is approximately material-independent.
    constexpr double p = 1.77;

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
        pt.dose_rel  = bortfeldDoseAtDepth(z, R0_cm, sigma_cm, p);
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
