#include "biosim/physics/StoppingPowerEngine.h"

#include <algorithm>
#include <cmath>

namespace beamlab::biosim {

namespace {

constexpr double k_BB    = 0.307075;    // [MeV·cm²/mol]
constexpr double m_e_MeV = 0.51099895;  // electron rest mass [MeV/c²]
constexpr double MeV_to_J = 1.602176634e-13;
constexpr double M_PI_val = 3.14159265358979323846;

// Sternheimer density-effect correction δ(x) where x = log10(β·γ).
// Returns 0 if mat.has_sternheimer is false.
double sternheimerDelta(const double log10_bg, const BioMaterial& mat)
{
    if (!mat.has_sternheimer) return 0.0;

    const double x  = log10_bg;
    const double x0 = mat.sternheimer_x0;
    const double x1 = mat.sternheimer_x1;
    const double a  = mat.sternheimer_a;
    const double k  = mat.sternheimer_k;
    const double C  = mat.sternheimer_C_delta;
    const double d0 = mat.sternheimer_delta0;

    if (x < x0) {
        return d0 * std::pow(10.0, 2.0 * (x - x0));
    }
    if (x < x1) {
        return 4.6052 * x + C + a * std::pow(x1 - x, k);
    }
    return 4.6052 * x + C;
}

// Bethe-Bloch mean mass stopping power [MeV·cm²/g].
// PDG 2022 eq. 34.5 with Sternheimer δ correction.
double betheBlochMass(const double kinE_MeV,
                       const double mass_MeV,
                       const double z_abs,    // |charge|
                       const BioMaterial& mat)
{
    const double I_MeV = mat.I_eV * 1.0e-6;
    const double gamma  = 1.0 + kinE_MeV / mass_MeV;
    const double beta2  = 1.0 - 1.0 / (gamma * gamma);

    if (beta2 <= 1.0e-10 || beta2 >= 1.0) return 0.0;

    const double beta = std::sqrt(beta2);
    const double bg   = beta * gamma;

    // Maximum energy transfer to free electron (PDG 34.4)
    const double M      = mass_MeV;
    const double Wmax   = (2.0 * m_e_MeV * bg * bg) /
                          (1.0 + 2.0 * gamma * m_e_MeV / M + (m_e_MeV / M) * (m_e_MeV / M));

    const double log_arg = 2.0 * m_e_MeV * bg * bg * Wmax / (I_MeV * I_MeV);
    if (log_arg <= 0.0) return 0.0;

    const double delta = sternheimerDelta(std::log10(bg), mat);
    const double ZA    = mat.Z_eff / mat.A_eff;
    const double z2    = z_abs * z_abs;

    const double sp = k_BB * z2 * ZA / beta2 *
                      (0.5 * std::log(log_arg) - beta2 - delta / 2.0);

    return std::max(0.0, sp);
}

} // namespace

// ── Public methods ────────────────────────────────────────────────────────────

double StoppingPowerEngine::massStoppingPower(
    const double kinE_MeV,
    const BioMaterial& mat,
    const ParticleSpecies& particle) const
{
    if (kinE_MeV <= 0.0) return 0.0;
    return betheBlochMass(kinE_MeV, particle.mass_MeV,
                          std::abs(particle.charge), mat);
}

double StoppingPowerEngine::dEdx_MeV_cm(
    const double kinE_MeV,
    const BioMaterial& mat,
    const ParticleSpecies& particle) const
{
    return massStoppingPower(kinE_MeV, mat, particle) * mat.density_g_cm3;
}

double StoppingPowerEngine::energyLoss_MeV(
    const double kinE_MeV,
    const double dx_cm,
    const BioMaterial& mat,
    const ParticleSpecies& particle) const
{
    if (kinE_MeV <= 0.0 || dx_cm <= 0.0) return 0.0;
    const double loss = dEdx_MeV_cm(kinE_MeV, mat, particle) * dx_cm;
    return std::min(loss, kinE_MeV); // particle cannot lose more than it has
}

double StoppingPowerEngine::dose_Gy(
    const double edep_MeV,
    const BioMaterial& mat,
    const double dx_cm,
    const double r_cm)
{
    if (edep_MeV <= 0.0 || dx_cm <= 0.0 || r_cm <= 0.0) return 0.0;
    const double vol_cm3 = M_PI_val * r_cm * r_cm * dx_cm;
    const double mass_kg = mat.density_g_cm3 * vol_cm3 * 1.0e-3;
    if (mass_kg <= 0.0) return 0.0;
    return (edep_MeV * MeV_to_J) / mass_kg;
}

double StoppingPowerEngine::mcsAngle_rad(
    const double kinE_MeV,
    const double dx_cm,
    const BioMaterial& mat,
    const ParticleSpecies& particle) const
{
    if (dx_cm <= 0.0 || mat.radiation_length_cm <= 0.0 || kinE_MeV <= 0.0) {
        return 0.0;
    }

    const double M     = particle.mass_MeV;
    const double z_abs = std::abs(particle.charge);
    const double gamma = 1.0 + kinE_MeV / M;
    const double beta2 = 1.0 - 1.0 / (gamma * gamma);
    if (beta2 <= 0.0) return 0.0;

    const double beta = std::sqrt(beta2);
    // Momentum p = M·β·γ [MeV/c], so β·p = M·β²·γ [MeV/c]
    const double beta_p_MeV = M * beta * gamma; // β·p  [MeV/c]

    const double t_over_X0 = dx_cm / mat.radiation_length_cm;
    if (t_over_X0 <= 0.0) return 0.0;

    // Highland-Lynch-Dahl formula (Lynch & Dahl 1991):
    //   θ₀ = (13.6 MeV / (β·p)) · |z| · √(t/X₀) · [1 + 0.038·ln(|z|²·t/X₀)]
    const double log_term = std::log(z_abs * z_abs * t_over_X0);
    const double theta0   = (13.6 / beta_p_MeV) * z_abs *
                             std::sqrt(t_over_X0) * (1.0 + 0.038 * log_term);

    return std::max(0.0, theta0);
}

double StoppingPowerEngine::mcsLateralDisplacement_cm(
    const double kinE_MeV,
    const double dx_cm,
    const BioMaterial& mat,
    const ParticleSpecies& particle) const
{
    const double theta0 = mcsAngle_rad(kinE_MeV, dx_cm, mat, particle);
    // Projected RMS lateral displacement: y = dx/√3 · θ₀
    return (dx_cm / std::sqrt(3.0)) * theta0;
}

} // namespace beamlab::biosim
