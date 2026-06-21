// C4: NistValidator headless test.
//
// Validates that the NistValidator comparator:
//   (a) identifies proton/water as having a NIST reference.
//   (b) domain deviations from NIST <= 1 % across the fixture grid (F1a already
//       showed ≤0.5 %; this gate is consistent with that).
//   (c) biosim deviations from NIST <= 1 % (F1b showed < 1 %).
//   (d) inter-engine deviations are populated even without a NIST reference.
//   (e) non-proton/non-water combinations set hasNistReference = false and
//       show the "inter-motor" note.
//   (f) fixture energies produce at least 10 rows (fixture integrity check).
//
// NistValidator is pure C++ (no Qt) and links directly into beamlab_tests.

#include "ui/validation/NistValidator.h"

#include "domain/materials/MaterialRegistry.h"
#include "domain/physics/ParticleRegistry.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <string>

using namespace beamlab::ui::validation;
using namespace beamlab::domain;

namespace {

constexpr double k_tolerance_1pct = 1.0;  // [%] tight gate for F1a/F1b results

class NistValidatorTest : public ::testing::Test {
protected:
    materials::MaterialRegistry matReg;
    physics::ParticleRegistry   partReg;

    void SetUp() override
    {
        matReg.loadBuiltinMaterials();
        partReg.loadBuiltinParticles();
    }
};

} // namespace

// ── (a) proton/water → hasNistReference = true ────────────────────────────

TEST_F(NistValidatorTest, test_proton_water_has_nist_reference)
{
    NistValidator::Options opts;
    opts.domainMaterialId = "water_icru";
    opts.domainParticleId = "proton";
    opts.biosimMaterialId = "water_icru";
    opts.biosimParticleId = "proton";

    NistValidator v(matReg, partReg);
    const auto res = v.validate(opts);

    EXPECT_TRUE(res.hasNistReference)
        << "proton/water must trigger NIST reference lookup";
}

// ── (b) domain deviations <= 1 % vs NIST for proton/water ────────────────

TEST_F(NistValidatorTest, test_domain_dedx_within_1pct_of_nist_proton_water)
{
    NistValidator::Options opts;
    opts.domainMaterialId = "water_icru";
    opts.domainParticleId = "proton";
    opts.biosimMaterialId = "";  // only test domain here
    opts.biosimParticleId = "";

    NistValidator v(matReg, partReg);
    const auto res = v.validate(opts);

    ASSERT_GE(res.rows.size(), 5u) << "Expected at least 5 NIST fixture rows";

    // Only test the clinical band 50–250 MeV where the formula is valid.
    int checked = 0;
    for (const auto& row : res.rows) {
        if (!row.has_nist || row.energy_MeV < 50.0 || row.energy_MeV > 250.0)
            continue;
        ASSERT_TRUE(row.domain_available)
            << "Domain engine unavailable at E=" << row.energy_MeV;

        EXPECT_LE(row.dev_domain_pct, k_tolerance_1pct)
            << "Domain deviation too large at E=" << row.energy_MeV
            << " MeV: " << row.dev_domain_pct << " %";
        ++checked;
    }
    EXPECT_GE(checked, 5) << "Expected at least 5 points in 50-250 MeV band";

    // Summary
    std::cout << "[C4] domain/NIST max dev (50-250 MeV): "
              << res.maxDevDomain_pct << " %\n";
}

// ── (c) biosim deviations <= 1 % vs NIST for proton/water ────────────────

TEST_F(NistValidatorTest, test_biosim_dedx_within_1pct_of_nist_proton_water)
{
    NistValidator::Options opts;
    opts.domainMaterialId = "water_icru";
    opts.domainParticleId = "proton";
    opts.biosimMaterialId = "water_icru";
    opts.biosimParticleId = "proton";

    NistValidator v(matReg, partReg);
    const auto res = v.validate(opts);

    int checked = 0;
    for (const auto& row : res.rows) {
        if (!row.has_nist || row.energy_MeV < 50.0 || row.energy_MeV > 250.0)
            continue;
        ASSERT_TRUE(row.biosim_available)
            << "BioSim engine unavailable at E=" << row.energy_MeV;

        EXPECT_LE(row.dev_biosim_pct, k_tolerance_1pct)
            << "BioSim deviation too large at E=" << row.energy_MeV
            << " MeV: " << row.dev_biosim_pct << " %";
        ++checked;
    }
    EXPECT_GE(checked, 5);

    std::cout << "[C4] biosim/NIST max dev (50-250 MeV): "
              << res.maxDevBiosim_pct << " %\n";
}

// ── (d) inter-engine deviations populated ─────────────────────────────────

TEST_F(NistValidatorTest, test_interengine_deviation_populated_proton_water)
{
    NistValidator::Options opts;
    opts.domainMaterialId = "water_icru";
    opts.domainParticleId = "proton";
    opts.biosimMaterialId = "water_icru";
    opts.biosimParticleId = "proton";

    NistValidator v(matReg, partReg);
    const auto res = v.validate(opts);

    // At least some rows should have inter-engine deviation computed.
    int populated = 0;
    for (const auto& row : res.rows) {
        if (row.domain_available && row.biosim_available && row.dev_interengine_pct > 0.0)
            ++populated;
    }
    EXPECT_GT(populated, 0) << "Expected at least one inter-engine deviation row";
}

// ── (e) non-proton/non-water → hasNistReference = false ──────────────────

TEST_F(NistValidatorTest, test_alpha_water_has_no_nist_reference)
{
    NistValidator::Options opts;
    opts.domainMaterialId = "water_icru";
    opts.domainParticleId = "alpha";
    opts.biosimMaterialId = "water_icru";
    opts.biosimParticleId = "alpha";

    NistValidator v(matReg, partReg);
    const auto res = v.validate(opts);

    EXPECT_FALSE(res.hasNistReference)
        << "alpha/water must NOT claim a NIST reference";
    EXPECT_NE(res.note.find("inter-motor"), std::string::npos)
        << "Note must mention 'inter-motor' for unsupported combinations";
}

// ── (f) proton/water produces >= 10 rows (fixture integrity) ─────────────

TEST_F(NistValidatorTest, test_proton_water_produces_at_least_10_fixture_rows)
{
    NistValidator::Options opts;
    opts.domainMaterialId = "water_icru";
    opts.domainParticleId = "proton";
    opts.biosimMaterialId = "water_icru";
    opts.biosimParticleId = "proton";

    NistValidator v(matReg, partReg);
    const auto res = v.validate(opts);

    EXPECT_GE(res.rows.size(), 10u)
        << "Fixture file must contain >= 10 proton/water data points";
}
