#include "domain/simulation/SimulationEngine.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace beamlab::domain::simulation {

namespace {

// ── Physical constants ─────────────────────────────────────────────

constexpr double k_BB       = 0.307075;    // Bethe-Bloch K [MeV·cm²/mol] (PDG 2022)
constexpr double m_e_MeV    = 0.51099895;  // electron rest mass [MeV/c²]
constexpr double k_MCS      = 13.6;        // Highland MCS scale [MeV]
constexpr double M_PI_v     = 3.14159265358979323846;
constexpr double c_cm_s     = 2.99792458e10; // speed of light [cm/s]

} // anonymous namespace

// ── Constructor ─────────────────────────────────────────────────────

SimulationEngine::SimulationEngine(
    const materials::MaterialRegistry& materials,
    const physics::ParticleRegistry& particles)
    : materials_(materials), particles_(particles)
{
}

// ── Sternheimer density-effect correction ──────────────────────────

double SimulationEngine::sternheimerDelta(double x,
                                          const materials::Material& mat) const
{
    const auto& s = mat.sternheimer;
    if (!s.hasData) return 0.0;

    if (x < s.x0) {
        return s.delta0 * std::pow(10.0, 2.0 * (x - s.x0));
    }
    if (x < s.x1) {
        return 4.6052 * x + s.C_delta + s.a * std::pow(s.x1 - x, s.k);
    }
    return 4.6052 * x + s.C_delta;
}

// ── Bethe-Bloch ────────────────────────────────────────────────────

double SimulationEngine::betheBloch_dEdx_MeV_cm(
    double kinE_MeV,
    const physics::Particle& particle,
    const materials::Material& material) const
{
    if (kinE_MeV <= 0.0) return 0.0;

    const double mass_MeV = particle.mass_MeV;
    const double z_abs = std::abs(particle.charge_e);
    if (z_abs < 1e-9) return 0.0;

    const double I_MeV = material.meanExcitationEnergy_eV * 1.0e-6;
    const double gamma = 1.0 + kinE_MeV / mass_MeV;
    const double beta2 = 1.0 - 1.0 / (gamma * gamma);

    if (beta2 <= 1.0e-10 || beta2 >= 1.0) return 0.0;

    const double beta = std::sqrt(beta2);
    const double bg   = beta * gamma;

    // Maximum energy transfer to free electron (PDG 34.4)
    const double M    = mass_MeV;
    const double Wmax = (2.0 * m_e_MeV * bg * bg) /
                        (1.0 + 2.0 * gamma * m_e_MeV / M
                         + (m_e_MeV / M) * (m_e_MeV / M));

    const double log_arg = 2.0 * m_e_MeV * bg * bg * Wmax / (I_MeV * I_MeV);
    if (log_arg <= 0.0) return 0.0;

    const double delta = sternheimerDelta(std::log10(bg), material);
    const double ZA = material.Z_eff / material.A_eff;
    const double z2 = z_abs * z_abs;

    double massSP = k_BB * z2 * ZA / beta2
                    * (0.5 * std::log(log_arg) - beta2 - delta / 2.0);
    massSP = std::max(0.0, massSP);

    // Convert mass stopping power [MeV·cm²/g] → linear [MeV/cm].
    return massSP * material.density_g_cm3;
}

// ── MCS (Highland) ──────────────────────────────────────────────────

double SimulationEngine::mcsAngle_rad(
    double kinE_MeV,
    double stepLength_cm,
    const physics::Particle& particle,
    const materials::Material& material) const
{
    if (kinE_MeV <= 0.0 || stepLength_cm <= 0.0) return 0.0;

    const double mass_MeV = particle.mass_MeV;
    const double p_MeV = std::sqrt(kinE_MeV * (kinE_MeV + 2.0 * mass_MeV));
    const double beta = p_MeV / (kinE_MeV + mass_MeV);
    const double z_abs = std::abs(particle.charge_e);
    if (z_abs < 1e-9 || p_MeV <= 0.0 || beta <= 0.0) return 0.0;

    const double X0_cm = material.radiationLength_cm;
    const double t = stepLength_cm / X0_cm;  // thickness in radiation lengths
    if (t <= 1e-12) return 0.0;

    // Highland (1975) / Lynch & Dahl (1991)
    double theta0 = (k_MCS / (beta * p_MeV)) * z_abs * std::sqrt(t)
                    * (1.0 + 0.038 * std::log(t));
    return theta0;
}

// ── Straggling (Bohr) ──────────────────────────────────────────────

double SimulationEngine::stragglingSigma_MeV(
    double kinE_MeV,
    double stepLength_cm,
    const physics::Particle& particle,
    const materials::Material& material) const
{
    if (kinE_MeV <= 0.0 || stepLength_cm <= 0.0) return 0.0;

    const double z_abs = std::abs(particle.charge_e);
    if (z_abs < 1e-9) return 0.0;

    // Bohr straggling: σ² = 4π·(e²)⁴·Nₑ·z²·dx  (simplified)
    // Using thin-absorber approximation:
    //   ξ = (K·z²·Z/A·ρ·dx) / (2·β²)                [MeV]
    const double mass_MeV = particle.mass_MeV;
    const double gamma = 1.0 + kinE_MeV / mass_MeV;
    const double beta2 = 1.0 - 1.0 / (gamma * gamma);
    if (beta2 <= 1e-10) return 0.0;

    const double xi = k_BB * z_abs * z_abs
                      * (material.Z_eff / material.A_eff)
                      * material.density_g_cm3 * stepLength_cm
                      / (2.0 * beta2);

    // For thick absorbers (κ = ξ / Wmax ≥ 10), Gaussian limit:
    // σ² = ξ · Wmax  →  σ = √(ξ · Wmax)
    // For simplicity return the Bohr estimator σ ≈ √(4π·Nₑ·z²·dx) · const
    // which is approximately: σ = √(2 · meanLoss · ε_max) simplified as:
    return 0.05 * xi;  // Approximate sigma — TODO: full Vavilov model
}

// ── computeStep ────────────────────────────────────────────────────

SimulationEngine::StepResult
SimulationEngine::computeStep(
    double kinE_MeV,
    double stepLength_cm,
    const std::string& materialName,
    const std::string& particleName) const
{
    const auto& material = materials_.get(materialName);
    const auto& particle = particles_.get(particleName);

    StepResult sr;
    sr.dEdx_MeV_cm = betheBloch_dEdx_MeV_cm(kinE_MeV, particle, material);
    sr.energyLoss_MeV = sr.dEdx_MeV_cm * stepLength_cm;
    // Causality: cannot lose more than the available kinetic energy,
    // and never a negative amount (unphysical kinE must not propagate).
    sr.energyLoss_MeV = std::min(sr.energyLoss_MeV, std::max(kinE_MeV, 0.0));

    sr.mcsAngle_rad = mcsAngle_rad(kinE_MeV, stepLength_cm,
                                    particle, material);
    // Highland model: projected RMS lateral displacement = dx/√3 · θ₀
    sr.mcsDisplacement_cm = stepLength_cm / std::sqrt(3.0) * sr.mcsAngle_rad;

    sr.stragglingSigma_MeV = stragglingSigma_MeV(kinE_MeV,
                                                  stepLength_cm,
                                                  particle, material);

    return sr;
}

// ── computeBraggCurve ──────────────────────────────────────────────

SimulationEngine::BraggCurve
SimulationEngine::computeBraggCurve(
    double initialE_MeV,
    const std::string& materialName,
    const std::string& particleName,
    std::size_t nSteps) const
{
    // Pre-validate.
    materials_.get(materialName);
    particles_.get(particleName);

    BraggCurve bc;
    bc.depth_cm.reserve(nSteps);
    bc.dEdx_MeV_cm.reserve(nSteps);

    const double dx_cm = 0.001;  // 10 μm step — fine enough for Bragg peak

    double E = initialE_MeV;
    double x = 0.0;

    double peakDepth = 0.0;
    double peakDEdx = 0.0;

    for (std::size_t i = 0; i < nSteps && E > 0.01; ++i) {
        auto step = computeStep(E, dx_cm, materialName, particleName);

        // Below the Bethe-Bloch validity range dE/dx evaluates to 0 and the
        // particle could never lose its remaining energy: treat it as stopped.
        if (step.dEdx_MeV_cm <= 0.0) {
            break;
        }

        bc.depth_cm.push_back(x);
        bc.dEdx_MeV_cm.push_back(step.dEdx_MeV_cm);

        if (step.dEdx_MeV_cm > peakDEdx) {
            peakDEdx = step.dEdx_MeV_cm;
            peakDepth = x;
        }

        E -= step.energyLoss_MeV;
        x += dx_cm;
    }

    bc.peakDepth_cm = peakDepth;
    bc.peakDEdx_MeV_cm = peakDEdx;

    return bc;
}

// ── validateAgainstNist ────────────────────────────────────────────

SimulationEngine::ValidationReport
SimulationEngine::validateAgainstNist(
    const std::string& materialName,
    const std::string& particleName) const
{
    // NIST PSTAR electronic stopping power for proton @ 150 MeV in
    // WATER, LIQUID (matno 276): 5.443 MeV·cm²/g × 1.0 g/cm³.
    // Our Bethe-Bloch (PDG 2022 Eq. 34.5, Sternheimer δ, no shell
    // corrections) gives ~5.40 MeV/cm — within 1% of PSTAR at this
    // energy, where shell corrections are negligible.
    constexpr double kRefStoppingPower_MeV_cm = 5.443;
    constexpr double kPassThresholdPct = 2.0;

    const double testE_MeV = 150.0;

    auto step = computeStep(testE_MeV, 1.0,
                            materialName, particleName);

    double ourSP = step.dEdx_MeV_cm;
    double deviationPct = 100.0 * std::abs(ourSP - kRefStoppingPower_MeV_cm)
                          / kRefStoppingPower_MeV_cm;

    ValidationReport report;
    report.referenceSource = "NIST PSTAR 2022 (Water, proton @ 150 MeV)";
    report.deviations.push_back({"dE/dx [MeV/cm]", deviationPct});
    report.passed = (deviationPct <= kPassThresholdPct);

    return report;
}

} // namespace beamlab::domain::simulation
