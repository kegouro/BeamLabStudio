#include "biosim/physics/EnergyStraggling.h"
#include "biosim/physics/StoppingPowerEngine.h"

#include <algorithm>
#include <cmath>

namespace beamlab::biosim {

namespace {
constexpr double k_BB    = 0.307075;    // [MeV·cm²/mol]
constexpr double m_e_MeV = 0.51099895;

// Uniform [0, 1) drawn from a 64-bit Mersenne Twister.  Using mt19937_64
// instead of the previous xorshift32 gives a longer period (2^19937-1 vs
// 2^32-1), better statistical quality (passes BigCrush), and lets us advance
// the same RNG state across millions of steps without revisiting samples.
double uniform01(std::mt19937_64& rng)
{
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    return dist(rng);
}

// Box-Muller transform → N(0,1)
double normalSample(std::mt19937_64& rng)
{
    double u1, u2;
    do { u1 = uniform01(rng); } while (u1 <= 1e-12);
    u2 = uniform01(rng);
    return std::sqrt(-2.0 * std::log(u1)) * std::cos(2.0 * M_PI * u2);
}

} // namespace

// ── Helpers ───────────────────────────────────────────────────────────────────

double EnergyStraggling::wmax_MeV(const double kinE_MeV, const double mass_MeV)
{
    const double gamma  = 1.0 + kinE_MeV / mass_MeV;
    const double beta2  = 1.0 - 1.0 / (gamma * gamma);
    const double bg2    = beta2 * gamma * gamma;
    const double M      = mass_MeV;
    return (2.0 * m_e_MeV * bg2) /
           (1.0 + 2.0 * gamma * m_e_MeV / M + (m_e_MeV / M) * (m_e_MeV / M));
}

double EnergyStraggling::xi_MeV(const double kinE_MeV,
                                  const double dx_cm,
                                  const BioMaterial& mat,
                                  const ParticleSpecies& particle) const
{
    if (kinE_MeV <= 0.0 || dx_cm <= 0.0) return 0.0;
    const double M     = particle.mass_MeV;
    const double gamma = 1.0 + kinE_MeV / M;
    const double beta2 = 1.0 - 1.0 / (gamma * gamma);
    if (beta2 <= 0.0) return 0.0;
    const double z2 = particle.charge * particle.charge;
    // ξ = K·z²·(Z/A)·ρ·dx / (2β²)  [MeV]
    return (k_BB * z2 * (mat.Z_eff / mat.A_eff) * mat.density_g_cm3 * dx_cm)
           / (2.0 * beta2);
}

double EnergyStraggling::kappa(const double kinE_MeV,
                                const double dx_cm,
                                const BioMaterial& mat,
                                const ParticleSpecies& particle) const
{
    const double W = wmax_MeV(kinE_MeV, particle.mass_MeV);
    if (W <= 0.0) return 0.0;
    return xi_MeV(kinE_MeV, dx_cm, mat, particle) / W;
}

double EnergyStraggling::bohrVariance_MeV2(const double kinE_MeV,
                                             const double dx_cm,
                                             const BioMaterial& mat,
                                             const ParticleSpecies& particle) const
{
    // Bohr energy-loss straggling variance (PDG 2022 §34.2.5; Bohr,
    // Rev.Mod.Phys. 40 (1968) 471):
    //
    //   σ² = ξ · Wmax · (1 − β²/2)            [MeV²]
    //
    // with ξ = K·z²·(Z/A)·ρ·dx / (2β²) [MeV] the Landau scale parameter
    // (same ξ as in xi_MeV).  Substituting ξ explicitly:
    //
    //   σ² = K·z²·(Z/A)·ρ·dx · Wmax · (1 − β²/2) / (2β²)
    //
    // The previous form divided by 2 instead of 2β², suppressing the
    // variance by exactly β² (≈0.1–0.4 over 50–250 MeV protons) and making
    // the Gaussian/Vavilov fluctuation far too narrow.
    if (kinE_MeV <= 0.0 || dx_cm <= 0.0) return 0.0;
    const double M      = particle.mass_MeV;
    const double gamma  = 1.0 + kinE_MeV / M;
    const double beta2  = 1.0 - 1.0 / (gamma * gamma);
    if (beta2 <= 0.0) return 0.0;
    const double W   = wmax_MeV(kinE_MeV, particle.mass_MeV);
    const double xi  = xi_MeV(kinE_MeV, dx_cm, mat, particle); // = K·z²·(Z/A)·ρ·dx/(2β²)
    return xi * W * (1.0 - beta2 / 2.0);
}

// ── Sampling ──────────────────────────────────────────────────────────────────

double EnergyStraggling::sampleLandau(const double xi,
                                       const double mean_loss,
                                       std::mt19937_64& rng) const
{
    // Moyal approximation to the Landau distribution (PDG 2022 §34.2.7).
    // Sample the standardised Moyal variate λ via inverse transform:
    //   λ = −ln(u₁) − ln(−ln(u₂))   with u₁, u₂ ~ U(0,1)
    // This λ has p.d.f. ∝ exp(−½(λ + e^(−λ))) (Moyal), the standard fast
    // approximation to Landau used in lieu of the full Landau inversion.
    //
    // Centering: E[λ] = 1 + γ_Euler ≈ 1.5772 for this generator, so to make
    // the sampled loss unbiased about the deterministic mean we shift by
    // −E[λ]; the fluctuation scale is ξ (Landau width parameter).  The old
    // code subtracted 0.2228 (the Landau *mode* offset, not the generator's
    // mean), which biased every step's loss high by ξ·1.354.
    constexpr double k_moyal_mean = 1.5772135; // 1 + Euler-Mascheroni γ
    double u1, u2;
    do { u1 = uniform01(rng); } while (u1 <= 1e-12);
    do { u2 = uniform01(rng); } while (u2 >= 1.0 - 1e-12);

    const double lam = -std::log(u1) - std::log(-std::log(u2));
    const double sample = xi * (lam - k_moyal_mean) + mean_loss;
    return std::max(0.0, sample);
}

double EnergyStraggling::sampleGaussian(const double mean,
                                         const double sigma,
                                         std::mt19937_64& rng) const
{
    return mean + sigma * normalSample(rng);
}

// ── Main entry point ──────────────────────────────────────────────────────────

StragglingResult EnergyStraggling::sample(
    const double kinE_MeV,
    const double dx_cm,
    const BioMaterial& mat,
    const ParticleSpecies& particle,
    const StragglingMode mode,
    std::mt19937_64& rng) const
{
    StragglingResult res;
    res.mode_used = mode;

    if (kinE_MeV <= 0.0 || dx_cm <= 0.0) return res;

    StoppingPowerEngine engine;
    const double dEdx  = engine.dEdx_MeV_cm(kinE_MeV, mat, particle);
    const double mean  = std::min(dEdx * dx_cm, kinE_MeV); // deterministic mean

    if (mode == StragglingMode::Deterministic) {
        res.energy_loss_MeV = mean;
        return res;
    }

    const double xi_val = xi_MeV(kinE_MeV, dx_cm, mat, particle);
    const double W      = wmax_MeV(kinE_MeV, particle.mass_MeV);
    const double k      = (W > 0.0) ? xi_val / W : 1e9;

    res.xi_MeV = xi_val;
    res.kappa  = k;

    double sampled = mean;

    if (mode == StragglingMode::Landau || k < 0.01) {
        res.mode_used = StragglingMode::Landau;
        sampled = sampleLandau(xi_val, mean, rng);

    } else if (mode == StragglingMode::Vavilov) {
        if (k < 0.01) {
            res.mode_used = StragglingMode::Landau;
            sampled = sampleLandau(xi_val, mean, rng);
        } else if (k >= 10.0) {
            // Gaussian regime
            const double sigma2 = bohrVariance_MeV2(kinE_MeV, dx_cm, mat, particle);
            const double sigma  = std::sqrt(std::max(0.0, sigma2));
            sampled = sampleGaussian(mean, sigma, rng);
            res.mode_used = StragglingMode::Vavilov; // label as Vavilov-equivalent
        } else {
            // Intermediate Vavilov regime: use Gaussian with Vavilov variance
            // (full Vavilov requires numerical inversion of characteristic function)
            const double sigma2 = bohrVariance_MeV2(kinE_MeV, dx_cm, mat, particle);
            const double sigma  = std::sqrt(std::max(0.0, sigma2));
            sampled = sampleGaussian(mean, sigma, rng);
            res.mode_used = StragglingMode::Vavilov;
        }
    }

    // Clamp: cannot lose more than kinetic energy, cannot gain energy
    if (!std::isfinite(sampled)) sampled = mean;
    res.energy_loss_MeV = std::clamp(sampled, 0.0, kinE_MeV);
    return res;
}

} // namespace beamlab::biosim
