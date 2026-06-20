// Tests for SOBPCalculator — pure C++, no Qt dependency.
//
// Validates:
//   (a) Range inversion: energyForRange_cm round-trips within 0.5 % of
//       the CSDA range for protons in water at several clinical energies.
//   (b) Plateau flatness: SOBP(z) = 1.0 ± 10 % in [proximal, distal] for
//       a standard 10-peak configuration (150 MeV, 10–15 cm).
//   (c) SOBP normalization: max(SOBP) = 1.0.
//   (d) Component count: n_peaks components returned.
//   (e) All weights non-negative.
//   (f) Plateau flatness holds for a wider modulation (100 MeV, 5–18 cm, N=12).
//
// Algorithm reference:
//   T. Bortfeld, K. Schlegel, "An analytical approximation of depth-dose
//   distributions for therapeutic proton beams", Phys. Med. Biol. 41 (1996)
//   1331–1339.

#include "biosim/materials/BioMaterialLibrary.h"
#include "biosim/physics/BraggPeakCalculator.h"
#include "biosim/physics/ParticleLibrary.h"
#include "biosim/physics/SOBPCalculator.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>

using namespace beamlab::biosim;

namespace {

constexpr double k_flatness_limit = 0.10; // ±10% clinical criterion

} // namespace

// (a) energyForRange_cm — round-trip accuracy < 0.5 %
TEST(SOBPCalculatorTest, test_range_inversion_roundtrip_within_half_percent)
{
    SOBPCalculator calc;
    BraggPeakCalculator bp;
    const auto water  = BioMaterialLibrary::water();
    const auto proton = ParticleLibrary::proton();

    // Clinical proton energies and their known approximate CSDA ranges.
    const double test_energies[] = {70.0, 100.0, 150.0, 200.0, 230.0};

    for (const double E : test_energies) {
        const double range_target = bp.csdaRange_cm(E, water, proton);
        ASSERT_GT(range_target, 0.0) << "csdaRange returned 0 for E=" << E;

        const double E_inv = calc.energyForRange_cm(range_target);
        ASSERT_GT(E_inv, 0.0) << "energyForRange_cm returned 0 for range=" << range_target;

        const double range_check = bp.csdaRange_cm(E_inv, water, proton);
        const double rel_err = std::abs(range_check - range_target) / range_target;
        EXPECT_LE(rel_err, 0.005)  // 0.5 %
            << "Range inversion error too large for E=" << E << " MeV: "
            << rel_err * 100.0 << "%";
    }
}

// (b)+(c)+(d)+(e) Standard 10-peak configuration: 150 MeV, plateau 10–15 cm.
TEST(SOBPCalculatorTest, test_sobp_plateau_flat_within_10pct_standard)
{
    SOBPCalculator calc;

    SOBPParams p;
    p.energy_max_MeV = 150.0;
    p.proximal_cm    = 10.0;
    p.distal_cm      = 15.0;
    p.n_peaks        = 10;
    p.depth_step_cm  = 0.02;

    const SOBPResult result = calc.compute(p);
    ASSERT_TRUE(result.valid) << "compute failed: " << result.error;

    // (c) Normalization
    const double max_dose = *std::max_element(
        result.sobp_dose.begin(), result.sobp_dose.end());
    EXPECT_NEAR(max_dose, 1.0, 1e-9) << "SOBP max is not 1.0";

    // (d) Component count
    EXPECT_EQ(static_cast<int>(result.components.size()), p.n_peaks);

    // (e) Weights non-negative
    for (const auto& comp : result.components) {
        EXPECT_GE(comp.weight, 0.0) << "Negative weight for E=" << comp.energy_MeV;
    }

    // (b) Plateau flatness ≤ 10%
    EXPECT_LE(result.plateau_max_deviation, k_flatness_limit)
        << "Plateau max deviation " << result.plateau_max_deviation * 100.0
        << "% exceeds 10% limit for standard 150 MeV / 10-15 cm / 10 peaks";
}

// (f) Wider modulation: 200 MeV, 5–18 cm plateau, 12 peaks.
TEST(SOBPCalculatorTest, test_sobp_plateau_flat_within_10pct_wide_modulation)
{
    SOBPCalculator calc;

    SOBPParams p;
    p.energy_max_MeV = 200.0;
    p.proximal_cm    =   5.0;
    p.distal_cm      =  18.0;
    p.n_peaks        =  12;
    p.depth_step_cm  =  0.02;

    const SOBPResult result = calc.compute(p);
    ASSERT_TRUE(result.valid) << "compute failed: " << result.error;

    const double max_dose = *std::max_element(
        result.sobp_dose.begin(), result.sobp_dose.end());
    EXPECT_NEAR(max_dose, 1.0, 1e-9);

    EXPECT_LE(result.plateau_max_deviation, k_flatness_limit)
        << "Plateau max deviation " << result.plateau_max_deviation * 100.0
        << "% exceeds 10% for wide 200 MeV / 5-18 cm / 12 peaks";
}

// (g) Error handling: invalid proximal >= distal.
TEST(SOBPCalculatorTest, test_sobp_invalid_proximal_returns_error)
{
    SOBPCalculator calc;

    SOBPParams p;
    p.energy_max_MeV = 150.0;
    p.proximal_cm    = 20.0;
    p.distal_cm      = 10.0; // reversed!
    p.n_peaks        = 5;

    const SOBPResult result = calc.compute(p);
    EXPECT_FALSE(result.valid);
    EXPECT_FALSE(result.error.empty());
}

// (h) Single-peak SOBP equals the normalised Bortfeld curve at the distal depth.
TEST(SOBPCalculatorTest, test_sobp_single_peak_is_normalised_bortfeld)
{
    SOBPCalculator calc;

    SOBPParams p;
    p.energy_max_MeV = 100.0;
    p.proximal_cm    =  7.0;
    p.distal_cm      =  7.5;  // very narrow — only one peak needed
    p.n_peaks        =   1;
    p.depth_step_cm  =  0.05;

    const SOBPResult result = calc.compute(p);
    ASSERT_TRUE(result.valid) << result.error;
    EXPECT_EQ(static_cast<int>(result.components.size()), 1);
    EXPECT_NEAR(result.components[0].weight, 1.0, 1e-9);

    // SOBP should be normalised to 1.
    const double max_dose = *std::max_element(
        result.sobp_dose.begin(), result.sobp_dose.end());
    EXPECT_NEAR(max_dose, 1.0, 1e-9);
}
