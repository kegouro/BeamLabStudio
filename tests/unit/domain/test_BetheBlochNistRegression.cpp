// Physics regression: Bethe-Bloch dE/dx for protons in water, validated
// against a REAL NIST PSTAR reference table (tests/fixtures/), not against
// the engine's own output (S5: kill the autoreferential validation).
//
// Targets (F1a brief):
//   (a) |dE/dx - NIST| <= 5 % for protons in water across 50-250 MeV.
//   (b) property: dE/dx is strictly monotone-decreasing with energy there.
//
// The fixture uses NIST PSTAR (I = 75 eV, ICRU-37/49). The engine uses the
// ICRU-90 I = 78 eV knob, which lowers dE/dx ~0.4 % vs PSTAR -- still well
// inside 5 %. Both standards are intentionally honoured at once.

#include "domain/simulation/SimulationEngine.h"
#include "domain/materials/MaterialRegistry.h"
#include "domain/physics/ParticleRegistry.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using namespace beamlab::domain::simulation;
using namespace beamlab::domain::materials;
using namespace beamlab::domain::physics;

namespace {

#ifndef PROJECT_SOURCE_DIR
#  error "PROJECT_SOURCE_DIR must be defined by the build for fixture lookup"
#endif

// One NIST PSTAR reference row.
struct NistRow {
    double energy_MeV;            // [MeV] proton kinetic energy
    double stoppingPower_MeV_cm2_g;  // [MeV*cm^2/g] total mass stopping power
};

// Parse tests/fixtures/nist_pstar_proton_water.csv. Lines starting with '#'
// or blank are skipped; the rest are "energy,stoppingPower".
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

class BetheBlochNistRegression : public ::testing::Test {
protected:
    MaterialRegistry matReg;
    ParticleRegistry partReg;
    std::unique_ptr<SimulationEngine> engine;

    void SetUp() override
    {
        matReg.loadBuiltinMaterials();
        partReg.loadBuiltinParticles();
        engine = std::make_unique<SimulationEngine>(matReg, partReg);
    }
};

constexpr double k_band_lo_MeV   = 50.0;   // [MeV] clinical proton band lower
constexpr double k_band_hi_MeV   = 250.0;  // [MeV] clinical proton band upper
constexpr double k_tolerance_pct = 5.0;    // [%] F1a target vs NIST PSTAR

} // namespace

// (a) Accuracy: every fixture point in 50-250 MeV must be within 5 % of NIST.
TEST_F(BetheBlochNistRegression,
       test_bethe_bloch_proton_water_within5pct_of_nist)
{
    const auto rows = loadNistFixture();
    ASSERT_GE(rows.size(), 10u) << "Fixture must hold >= 10 reference points";

    const double rho = matReg.get("water_icru").density_g_cm3;  // [g/cm^3]

    int checked = 0;
    double worstDevPct = 0.0;
    double worstE = 0.0;

    for (const auto& r : rows) {
        if (r.energy_MeV < k_band_lo_MeV || r.energy_MeV > k_band_hi_MeV)
            continue;

        // NIST gives mass stopping power; convert to linear dE/dx [MeV/cm].
        const double ref_dEdx_MeV_cm = r.stoppingPower_MeV_cm2_g * rho;

        const auto step =
            engine->computeStep(r.energy_MeV, 0.01, "water_icru", "proton");
        const double our_dEdx_MeV_cm = step.dEdx_MeV_cm;  // [MeV/cm]

        const double devPct =
            100.0 * std::abs(our_dEdx_MeV_cm - ref_dEdx_MeV_cm) / ref_dEdx_MeV_cm;
        if (devPct > worstDevPct) { worstDevPct = devPct; worstE = r.energy_MeV; }
        ++checked;

        EXPECT_LE(devPct, k_tolerance_pct)
            << "E=" << r.energy_MeV << " MeV: NIST=" << ref_dEdx_MeV_cm
            << " ours=" << our_dEdx_MeV_cm << " dev=" << devPct << "%";
    }

    EXPECT_GE(checked, 5) << "Expected >= 5 fixture points inside 50-250 MeV";
    std::cout << "[NIST regression] proton/water 50-250 MeV: worst deviation "
              << worstDevPct << "% at " << worstE << " MeV (limit "
              << k_tolerance_pct << "%)\n";
}

// (b) Property-based: dE/dx strictly decreases with energy in 50-250 MeV.
// Sweep 50->250 MeV; each higher energy must yield strictly lower dE/dx
// (1/beta^2 dominance below the relativistic rise). Independent of the
// fixture's discrete sampling.
TEST_F(BetheBlochNistRegression,
       test_dedx_monotone_decreasing_proton_water_50_to_250_MeV)
{
    double prev_dEdx = std::numeric_limits<double>::infinity();  // [MeV/cm]
    double prevE = k_band_lo_MeV;

    for (double E = k_band_lo_MeV; E <= k_band_hi_MeV + 1e-9; E += 5.0) {
        const auto step = engine->computeStep(E, 0.01, "water_icru", "proton");
        const double dEdx = step.dEdx_MeV_cm;  // [MeV/cm]

        EXPECT_GT(dEdx, 0.0) << "dE/dx must be positive at " << E << " MeV";
        EXPECT_LT(dEdx, prev_dEdx)
            << "dE/dx must strictly decrease: at " << E << " MeV dEdx=" << dEdx
            << " >= dEdx(" << prevE << " MeV)=" << prev_dEdx;

        prev_dEdx = dEdx;
        prevE = E;
    }
}

// Sanity: the fixture's self-audit hash invariant is documented in-file; here
// we assert the parsed table is well-formed (sorted, positive, in range).
TEST_F(BetheBlochNistRegression, test_nist_fixture_is_wellformed)
{
    const auto rows = loadNistFixture();
    ASSERT_GE(rows.size(), 10u);

    for (std::size_t i = 0; i < rows.size(); ++i) {
        EXPECT_GT(rows[i].energy_MeV, 0.0);
        EXPECT_GT(rows[i].stoppingPower_MeV_cm2_g, 0.0);
        if (i > 0) {
            EXPECT_GT(rows[i].energy_MeV, rows[i - 1].energy_MeV)
                << "Fixture energies must be strictly ascending";
            EXPECT_LT(rows[i].stoppingPower_MeV_cm2_g,
                      rows[i - 1].stoppingPower_MeV_cm2_g)
                << "NIST stopping power must decrease with energy";
        }
    }
}
