// Physics regression for the BIOSIM stopping-power path (BioSimRunner →
// StoppingPowerEngine + EnergyStraggling), validated against the REAL NIST
// PSTAR proton-in-water reference table (tests/fixtures/), not against the
// engine's own output.
//
// Context (F1b): the domain SimulationEngine was already validated to ~0.5 %
// vs NIST in F1a. This guards the *parallel* biosim implementation, which
// drives the end-to-end UI route, against the same standard.
//
// These classes (StoppingPowerEngine, EnergyStraggling, BioMaterialLibrary,
// ParticleLibrary) are pure physics — no Qt — so they link straight into the
// headless beamlab_tests executable as direct sources.
//
// Targets:
//   (a) |dE/dx − NIST| ≤ 5 % for protons in water across 50–250 MeV
//       (achieved: < 1 %).
//   (b) property: mass stopping power strictly decreases with energy there.
//   (c) straggling: Bohr variance σ² = ξ·Wmax·(1−β²/2) grows monotonically
//       (∝ thickness) and the sampled loss is unbiased about the mean.

#include "biosim/materials/BioMaterialLibrary.h"
#include "biosim/physics/EnergyStraggling.h"
#include "biosim/physics/ParticleLibrary.h"
#include "biosim/physics/StoppingPowerEngine.h"

#include <gtest/gtest.h>

#include <cmath>
#include <fstream>
#include <limits>
#include <random>
#include <sstream>
#include <string>
#include <vector>

using namespace beamlab::biosim;

namespace {

#ifndef PROJECT_SOURCE_DIR
#  error "PROJECT_SOURCE_DIR must be defined by the build for fixture lookup"
#endif

struct NistRow {
    double energy_MeV;               // [MeV] proton kinetic energy
    double stoppingPower_MeV_cm2_g;  // [MeV·cm²/g] total mass stopping power
};

// Parse tests/fixtures/nist_pstar_proton_water.csv; skip '#'/blank lines.
std::vector<NistRow> loadNistFixture()
{
    const std::string path =
        std::string(PROJECT_SOURCE_DIR) +
        "/tests/fixtures/nist_pstar_proton_water.csv";

    std::ifstream in(path);
    EXPECT_TRUE(in.is_open()) << "Cannot open NIST fixture: " << path;

    std::vector<NistRow> rows;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::stringstream ss(line);
        std::string eStr, spStr;
        if (!std::getline(ss, eStr, ',')) continue;
        if (!std::getline(ss, spStr, ',')) continue;
        rows.push_back({std::stod(eStr), std::stod(spStr)});
    }
    return rows;
}

constexpr double k_band_lo_MeV   = 50.0;   // [MeV] clinical proton band lower
constexpr double k_band_hi_MeV   = 250.0;  // [MeV] clinical proton band upper
constexpr double k_tolerance_pct = 5.0;    // [%] F1b target vs NIST PSTAR

} // namespace

// (a) Accuracy: every fixture point in 50–250 MeV within 5 % of NIST.
TEST(BiosimStoppingPowerNistRegression,
     test_bethe_bloch_proton_water_within5pct_of_nist)
{
    const auto rows   = loadNistFixture();
    ASSERT_GE(rows.size(), 10u) << "Fixture must hold ≥ 10 reference points";

    const auto water  = BioMaterialLibrary::water();
    const auto proton = ParticleLibrary::proton();
    StoppingPowerEngine engine;

    int    checked      = 0;
    double worstDevPct  = 0.0;
    double worstE       = 0.0;

    for (const auto& r : rows) {
        if (r.energy_MeV < k_band_lo_MeV || r.energy_MeV > k_band_hi_MeV)
            continue;

        // Compare mass stopping power directly (NIST units: MeV·cm²/g).
        const double ours = engine.massStoppingPower(r.energy_MeV, water, proton);
        const double devPct =
            100.0 * std::abs(ours - r.stoppingPower_MeV_cm2_g) /
            r.stoppingPower_MeV_cm2_g;

        if (devPct > worstDevPct) { worstDevPct = devPct; worstE = r.energy_MeV; }
        ++checked;

        EXPECT_LE(devPct, k_tolerance_pct)
            << "E=" << r.energy_MeV << " MeV: NIST="
            << r.stoppingPower_MeV_cm2_g << " ours=" << ours
            << " dev=" << devPct << "%";
    }

    EXPECT_GE(checked, 5) << "Expected ≥ 5 fixture points inside 50–250 MeV";
    std::cout << "[biosim NIST regression] proton/water 50–250 MeV: worst "
              << "deviation " << worstDevPct << "% at " << worstE
              << " MeV (limit " << k_tolerance_pct << "%)\n";
}

// (b) Property: mass stopping power strictly decreases with energy in band.
TEST(BiosimStoppingPowerNistRegression,
     test_dedx_monotone_decreasing_proton_water_50_to_250_MeV)
{
    const auto water  = BioMaterialLibrary::water();
    const auto proton = ParticleLibrary::proton();
    StoppingPowerEngine engine;

    double prev_sp = std::numeric_limits<double>::infinity();
    double prevE   = k_band_lo_MeV;

    for (double E = k_band_lo_MeV; E <= k_band_hi_MeV + 1e-9; E += 5.0) {
        const double sp = engine.massStoppingPower(E, water, proton);
        EXPECT_GT(sp, 0.0) << "stopping power must be positive at " << E << " MeV";
        EXPECT_LT(sp, prev_sp)
            << "stopping power must strictly decrease: at " << E
            << " MeV sp=" << sp << " ≥ sp(" << prevE << ")=" << prev_sp;
        prev_sp = sp;
        prevE   = E;
    }
}

// (c1) Straggling property: Bohr variance grows monotonically with thickness
// and obeys the σ² ∝ dx law (σ/√dx is constant for fixed energy).
TEST(BiosimEnergyStraggling, test_bohr_variance_grows_with_thickness)
{
    const auto water  = BioMaterialLibrary::water();
    const auto proton = ParticleLibrary::proton();
    EnergyStraggling strag;

    constexpr double E = 100.0; // MeV
    double prev_var = -1.0;
    double ref_sigma_over_sqrt_dx = -1.0;

    for (double dx : {0.01, 0.02, 0.05, 0.10, 0.20, 0.50}) {
        const double var = strag.bohrVariance_MeV2(E, dx, water, proton);
        EXPECT_GT(var, 0.0) << "variance must be positive at dx=" << dx;
        EXPECT_GT(var, prev_var) << "variance must grow with dx; dx=" << dx;

        // σ²/dx is energy-only ⇒ σ/√dx constant across dx.
        const double s_over_sqrt = std::sqrt(var) / std::sqrt(dx);
        if (ref_sigma_over_sqrt_dx < 0.0) {
            ref_sigma_over_sqrt_dx = s_over_sqrt;
        } else {
            EXPECT_NEAR(s_over_sqrt, ref_sigma_over_sqrt_dx,
                        ref_sigma_over_sqrt_dx * 1e-9)
                << "σ ∝ √dx law violated at dx=" << dx;
        }
        prev_var = var;
    }
}

// (c2) Straggling correctness: bohrVariance_MeV2 reproduces the canonical
// Bohr form σ² = ξ·Wmax·(1−β²/2) (guards the β²-suppression regression).
TEST(BiosimEnergyStraggling, test_bohr_variance_matches_canonical_formula)
{
    const auto water  = BioMaterialLibrary::water();
    const auto proton = ParticleLibrary::proton();
    EnergyStraggling strag;

    constexpr double K  = 0.307075;    // [MeV·cm²/mol]
    constexpr double me = 0.51099895;  // [MeV]
    const double M  = proton.mass_MeV;
    const double z2 = proton.charge * proton.charge;
    const double dx = 0.1;             // cm

    for (double E : {50.0, 100.0, 150.0, 200.0, 250.0}) {
        const double gamma = 1.0 + E / M;
        const double beta2 = 1.0 - 1.0 / (gamma * gamma);
        const double bg2   = beta2 * gamma * gamma;
        const double Wmax  = (2.0 * me * bg2) /
                             (1.0 + 2.0 * gamma * me / M + (me / M) * (me / M));
        const double xi    = (K * z2 * (water.Z_eff / water.A_eff) *
                              water.density_g_cm3 * dx) / (2.0 * beta2);
        const double canonical = xi * Wmax * (1.0 - beta2 / 2.0);

        const double got = strag.bohrVariance_MeV2(E, dx, water, proton);
        EXPECT_NEAR(got, canonical, canonical * 1e-9)
            << "Bohr variance mismatch at E=" << E << " MeV";
    }
}

// (c3) Straggling sampler: the sampled energy loss is unbiased about the
// deterministic mean (Landau & Vavilov), and Vavilov width matches Bohr σ.
TEST(BiosimEnergyStraggling, test_sampled_loss_is_unbiased_about_mean)
{
    const auto water  = BioMaterialLibrary::water();
    const auto proton = ParticleLibrary::proton();
    EnergyStraggling   strag;
    StoppingPowerEngine engine;

    constexpr double E  = 100.0; // MeV
    constexpr double dx = 0.1;   // cm
    constexpr int    N  = 100000;

    const double mean_loss = engine.energyLoss_MeV(E, dx, water, proton);
    ASSERT_GT(mean_loss, 0.0);

    for (auto mode : {StragglingMode::Landau, StragglingMode::Vavilov}) {
        std::mt19937_64 rng(20260620ull);
        double sum = 0.0, sumsq = 0.0;
        for (int i = 0; i < N; ++i) {
            const auto r = strag.sample(E, dx, water, proton, mode, rng);
            sum   += r.energy_loss_MeV;
            sumsq += r.energy_loss_MeV * r.energy_loss_MeV;
        }
        const double emp_mean = sum / N;
        // Mean within 2 % of the deterministic loss (clamp at 0 biases up a
        // hair; the dominant ξ·1.354 bias regression is ~9 %).
        EXPECT_NEAR(emp_mean, mean_loss, mean_loss * 0.02)
            << "mode=" << static_cast<int>(mode)
            << " emp_mean=" << emp_mean << " mean_loss=" << mean_loss;

        const double emp_var = sumsq / N - emp_mean * emp_mean;
        EXPECT_GT(emp_var, 0.0) << "sampled loss must fluctuate";
    }

    // Vavilov empirical σ should track the Bohr σ for this thin step.
    std::mt19937_64 rng(987654321ull);
    double sum = 0.0, sumsq = 0.0;
    for (int i = 0; i < N; ++i) {
        const auto r = strag.sample(E, dx, water, proton,
                                    StragglingMode::Vavilov, rng);
        sum   += r.energy_loss_MeV;
        sumsq += r.energy_loss_MeV * r.energy_loss_MeV;
    }
    const double emp_sigma = std::sqrt(sumsq / N - (sum / N) * (sum / N));
    const double bohr_sigma =
        std::sqrt(strag.bohrVariance_MeV2(E, dx, water, proton));
    EXPECT_NEAR(emp_sigma, bohr_sigma, bohr_sigma * 0.10)
        << "Vavilov-Gaussian σ should match Bohr σ within 10 %";
}
