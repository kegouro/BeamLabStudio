#include "domain/simulation/SimulationEngine.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace beamlab::domain::simulation {

namespace {

// ── Physical constants ─────────────────────────────────────────────
//
// Bethe-Bloch coefficient K = 4*pi*N_A*r_e^2*m_e*c^2. Carrying K [MeV*cm^2/mol]
// against Z/A [mol/g] yields a mass stopping power [MeV*cm^2/g]; multiplying by
// density [g/cm^3] gives linear dE/dx [MeV/cm].

constexpr double k_BB       = 0.307075;    // [MeV*cm^2/mol] PDG 2024 Eq. (34.5)
constexpr double m_e_MeV    = 0.51099895;  // [MeV] electron rest energy (PDG)
constexpr double k_MCS      = 13.6;        // [MeV] Highland MCS scale (PDG)

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

    const double mass_MeV = particle.mass_MeV;   // [MeV] from registry (PDG)
    const double z_abs = std::abs(particle.charge_e);  // [e]
    if (z_abs < 1e-9) return 0.0;

    // I-value knob: the mean excitation energy is taken straight from the
    // material (config/registry), never hardcoded here. For liquid water it is
    // 78 eV (ICRU-90, 2016). Swapping the material's I is the single point of
    // control for the dominant systematic in this formula.
    // ponytail: no Sternheimer-shell / Barkas correction yet | techo: matches
    // NIST PSTAR to <=0.5% only in ~10-300 MeV (low-E shell + high-E density
    // effect unmodelled) | upgrade: F1b adds shell + tuned Sternheimer delta.
    const double I_MeV = material.meanExcitationEnergy_eV * 1.0e-6;  // eV -> MeV
    const double gamma = 1.0 + kinE_MeV / mass_MeV;  // [dimensionless] E/m + 1
    const double beta2 = 1.0 - 1.0 / (gamma * gamma);  // [dimensionless] (v/c)^2

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
    // Pre-validate: throws std::out_of_range if not found.
    // Results discarded intentionally — only the side-effect (throw) matters.
    (void)materials_.get(materialName);
    (void)particles_.get(particleName);

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
//
// S5 fix: this used to compare a single self-derived 150 MeV value, i.e. it
// effectively validated the engine against itself. It now checks the computed
// dE/dx against a small EXTERNAL table of published NIST PSTAR reference
// values across the clinical 50-250 MeV proton band. (The exhaustive,
// hash-audited reference table lives in tests/fixtures/nist_pstar_proton_water
// .csv; this embedded subset keeps the domain layer free of test-fixture I/O.)

SimulationEngine::ValidationReport
SimulationEngine::validateAgainstNist(
    const std::string& materialName,
    const std::string& particleName) const
{
    // NIST PSTAR total mass stopping power [MeV*cm^2/g] for PROTON in
    // WATER, LIQUID (matno 276, I=75 eV ICRU-37/49), from
    // physics.nist.gov/cgi-bin/Star/ap_table.pl. Nuclear stopping is
    // < 0.05 % here, so total ~= electronic.
    struct RefPoint { double E_MeV; double massSP_MeV_cm2_g; };  // [MeV],[MeV*cm^2/g]
    static constexpr RefPoint k_nist_pstar_water[] = {
        {  50.0, 12.45  },   // NIST PSTAR
        { 100.0,  7.289 },   // NIST PSTAR
        { 150.0,  5.445 },   // NIST PSTAR
        { 200.0,  4.492 },   // NIST PSTAR
        { 250.0,  3.911 },   // NIST PSTAR
    };
    // F1a target: <= 5 % across the band. (Engine uses ICRU-90 I=78 eV, ~0.4 %
    // below the I=75 PSTAR reference -- comfortably inside tolerance.)
    constexpr double k_pass_threshold_pct = 5.0;  // [%]

    const auto& material = materials_.get(materialName);
    const double rho_g_cm3 = material.density_g_cm3;  // [g/cm^3]

    ValidationReport report;
    report.referenceSource =
        "NIST PSTAR (Water, Liquid; proton; 50-250 MeV; I=75 eV ICRU-37/49)";
    report.passed = true;

    double worst_dev_pct = 0.0;  // [%]
    for (const auto& ref : k_nist_pstar_water) {
        const double ref_dEdx_MeV_cm = ref.massSP_MeV_cm2_g * rho_g_cm3;  // [MeV/cm]
        const auto step = computeStep(ref.E_MeV, 1.0, materialName, particleName);
        const double our_dEdx_MeV_cm = step.dEdx_MeV_cm;  // [MeV/cm]

        const double dev_pct = ref_dEdx_MeV_cm > 0.0
            ? 100.0 * std::abs(our_dEdx_MeV_cm - ref_dEdx_MeV_cm) / ref_dEdx_MeV_cm
            : 0.0;  // [%]

        report.deviations.push_back(
            {"dE/dx @ " + std::to_string(static_cast<int>(ref.E_MeV)) + " MeV [%]",
             dev_pct});
        worst_dev_pct = std::max(worst_dev_pct, dev_pct);
        if (dev_pct > k_pass_threshold_pct) report.passed = false;
    }

    // Front entry = worst-case deviation, so callers reading deviations.front()
    // see the binding number rather than an arbitrary energy point.
    report.deviations.insert(report.deviations.begin(),
                             {"max dev over 50-250 MeV [%]", worst_dev_pct});
    return report;
}

} // namespace beamlab::domain::simulation
