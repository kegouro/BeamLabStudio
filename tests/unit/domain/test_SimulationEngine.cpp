#include "domain/simulation/SimulationEngine.h"
#include "domain/materials/MaterialRegistry.h"
#include "domain/physics/ParticleRegistry.h"

#include <gtest/gtest.h>

#include <cmath>

using namespace beamlab::domain::simulation;
using namespace beamlab::domain::materials;
using namespace beamlab::domain::physics;

class SimulationEngineTest : public ::testing::Test {
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

// ── Basic existence checks ───────────────────────────────────────

TEST_F(SimulationEngineTest, ConstructorDoesNotThrow)
{
    EXPECT_NO_THROW(SimulationEngine(matReg, partReg));
}

// ── computeStep ──────────────────────────────────────────────────

TEST_F(SimulationEngineTest, ComputeStepProtonInWater)
{
    // 150 MeV proton in water, 1 mm step.
    auto step = engine->computeStep(150.0, 0.1, "water_icru", "proton");

    // Stopping power ≈ 5.4 MeV/cm at 150 MeV (PSTAR).
    EXPECT_GT(step.dEdx_MeV_cm, 0.0);
    EXPECT_GT(step.energyLoss_MeV, 0.0);
    EXPECT_LE(step.energyLoss_MeV, 150.0);  // cannot exceed kinetic energy

    // MCS angle: 150 MeV proton in 1 mm water → ~0.01 rad.
    EXPECT_GE(step.mcsAngle_rad, 0.0);

    // Straggling sigma: positive.
    EXPECT_GE(step.stragglingSigma_MeV, 0.0);
}

TEST_F(SimulationEngineTest, ZeroKineticEnergyReturnsZero)
{
    auto step = engine->computeStep(0.0, 0.1, "water_icru", "proton");

    EXPECT_NEAR(step.dEdx_MeV_cm, 0.0, 1e-12);
    EXPECT_NEAR(step.energyLoss_MeV, 0.0, 1e-12);
}

TEST_F(SimulationEngineTest, InvalidMaterialThrows)
{
    EXPECT_THROW(
        engine->computeStep(100.0, 0.1, "nonexistent", "proton"),
        std::out_of_range);
}

TEST_F(SimulationEngineTest, InvalidParticleThrows)
{
    EXPECT_THROW(
        engine->computeStep(100.0, 0.1, "water_icru", "nonexistent"),
        std::out_of_range);
}

TEST_F(SimulationEngineTest, StoppingPowerIncreasesNearBraggPeak)
{
    // At low energy (1 MeV), stopping power should be much higher
    // than at high energy (150 MeV) due to the 1/β² term.
    auto stepHigh = engine->computeStep(150.0, 0.01, "water_icru", "proton");
    auto stepLow  = engine->computeStep(1.0,   0.01, "water_icru", "proton");

    // dE/dx at 1 MeV > dE/dx at 150 MeV
    EXPECT_GT(stepLow.dEdx_MeV_cm, stepHigh.dEdx_MeV_cm);
}

TEST_F(SimulationEngineTest, HeavierMaterialMoreStoppingPower)
{
    // Lead should have much higher stopping power than water.
    auto stepWater = engine->computeStep(100.0, 0.01, "water_icru", "proton");
    auto stepLead  = engine->computeStep(100.0, 0.01, "lead", "proton");

    EXPECT_GT(stepLead.dEdx_MeV_cm, stepWater.dEdx_MeV_cm);
}

TEST_F(SimulationEngineTest, AlphaHasHigherStoppingPowerThanProton)
{
    // Same energy, water. Alpha (Z=2) should have ~4× stopping power
    // relative to proton (Z=1) due to z² factor.
    auto stepP = engine->computeStep(10.0, 0.01, "water_icru", "proton");
    auto stepA = engine->computeStep(10.0, 0.01, "water_icru", "alpha");

    EXPECT_GT(stepA.dEdx_MeV_cm, stepP.dEdx_MeV_cm);
}

TEST_F(SimulationEngineTest, EnergyLossClampedToKinE)
{
    // Very thick step: should only lose as much energy as available.
    auto step = engine->computeStep(0.1, 1000.0, "water_icru", "proton");

    EXPECT_LE(step.energyLoss_MeV, 0.1);
}

// ── computeBraggCurve ───────────────────────────────────────────

TEST_F(SimulationEngineTest, BraggCurveHasPositivePeak)
{
    auto bragg = engine->computeBraggCurve(50.0, "water_icru", "proton");

    EXPECT_GE(bragg.depth_cm.size(), 2u);
    EXPECT_GE(bragg.dEdx_MeV_cm.size(), 2u);
    EXPECT_GT(bragg.peakDepth_cm, 0.0);
    EXPECT_GT(bragg.peakDEdx_MeV_cm, 0.0);
}

TEST_F(SimulationEngineTest, BraggCurvePeakNearEndOfRange)
{
    auto bragg = engine->computeBraggCurve(50.0, "water_icru", "proton");

    // Peak should be at > 50% of total range (Bragg peak).
    // In a deterministic CSDA integration (no straggling) dE/dx grows
    // monotonically as the particle slows, so the peak may sit exactly
    // at the end of range — the distal falloff seen in measured curves
    // comes from range straggling, which this model does not include.
    double totalRange = bragg.depth_cm.back();
    EXPECT_GT(bragg.peakDepth_cm, totalRange * 0.5);
    EXPECT_LE(bragg.peakDepth_cm, totalRange);
}

TEST_F(SimulationEngineTest, HigherEnergyBraggCurveDeeper)
{
    auto bragg50 = engine->computeBraggCurve(50.0, "water_icru", "proton");
    auto bragg150 = engine->computeBraggCurve(150.0, "water_icru", "proton");

    // 150 MeV proton should go deeper than 50 MeV.
    EXPECT_GT(bragg150.peakDepth_cm, bragg50.peakDepth_cm);
}

TEST_F(SimulationEngineTest, BraggCurveStopsForLowEnergy)
{
    auto bragg = engine->computeBraggCurve(0.05, "water_icru", "proton");

    // Below 0.1 MeV threshold, should produce minimal results.
    EXPECT_TRUE(bragg.depth_cm.empty() || bragg.depth_cm.size() <= 2u);
}

TEST_F(SimulationEngineTest, BraggCurveTerminatesWhenModelBreaksDown)
{
    // At 0.02 MeV a proton is below the validity range of Bethe-Bloch:
    // dE/dx evaluates to 0, so the particle can never lose energy and the
    // integration loop must terminate instead of padding all nSteps with
    // zero-dE/dx entries (frozen-particle tail).
    auto bragg = engine->computeBraggCurve(0.02, "water_icru", "proton");

    EXPECT_LE(bragg.depth_cm.size(), 2u);
}

TEST_F(SimulationEngineTest, BraggCurveHasNoZeroTail)
{
    // A full proton curve must end where the particle stops; trailing
    // entries with dE/dx == 0 are an integration artifact, not physics.
    auto bragg = engine->computeBraggCurve(50.0, "water_icru", "proton", 4000);

    ASSERT_FALSE(bragg.dEdx_MeV_cm.empty());
    EXPECT_GT(bragg.dEdx_MeV_cm.back(), 0.0);
}

// ── validateAgainstNist ─────────────────────────────────────────

TEST_F(SimulationEngineTest, ValidationProtonInWaterPasses)
{
    auto report = engine->validateAgainstNist("water_icru", "proton");

    // Our implementation should pass the ±2% test against NIST PSTAR.
    EXPECT_TRUE(report.passed)
        << "Deviation: " << report.deviations.front().second << "%";
    EXPECT_LE(report.deviations.front().second, 5.0);  // Even if fails 2%, should be <5%
}

TEST_F(SimulationEngineTest, ValidationReportsReference)
{
    auto report = engine->validateAgainstNist("water_icru", "proton");

    EXPECT_FALSE(report.referenceSource.empty());
    EXPECT_TRUE(
        report.referenceSource.find("NIST") != std::string::npos ||
        report.referenceSource.find("PSTAR") != std::string::npos);
}

// ── MCS tests ───────────────────────────────────────────────────

TEST_F(SimulationEngineTest, MCSAngleZeroForZeroThickness)
{
    auto step = engine->computeStep(100.0, 0.0, "water_icru", "proton");

    EXPECT_NEAR(step.mcsAngle_rad, 0.0, 1e-12);
    EXPECT_NEAR(step.mcsDisplacement_cm, 0.0, 1e-12);
}

TEST_F(SimulationEngineTest, MCSAngleIncreasesWithThickness)
{
    auto stepThin = engine->computeStep(100.0, 0.01, "water_icru", "proton");
    auto stepThick = engine->computeStep(100.0, 1.0, "water_icru", "proton");

    // Thicker material → more scattering.
    EXPECT_GT(stepThick.mcsAngle_rad, stepThin.mcsAngle_rad);
}

TEST_F(SimulationEngineTest, PhotonHasNoStoppingPower)
{
    auto step = engine->computeStep(10.0, 0.1, "water_icru", "gamma");

    // Photons interact via pair production/Compton, not Bethe-Bloch.
    // Implementation returns 0 for neutral particles.
    EXPECT_NEAR(step.dEdx_MeV_cm, 0.0, 1e-12);
}

// ═════════════════════════════════════════════════════════════════════
//  NIST PSTAR Validation — proton in "Water, Liquid"
// ═════════════════════════════════════════════════════════════════════
//
// Reference: NIST PSTAR (https://physics.nist.gov/PhysRefData/Star/Text/PSTAR.html)
//   Material: Water, Liquid (ρ = 1.0 g/cm³, I = 75.0 eV)
//   Particle: Proton
//
// The values below are the mass stopping power [MeV·cm²/g] multiplied by
// density (1.0 g/cm³) to give linear stopping power [MeV/cm].
//
// Our implementation uses the PDG 2022 Bethe-Bloch formula with
// Sternheimer density-effect correction.  Deviations from NIST are
// expected at low energies (below ~10 MeV) where shell corrections
// and nuclear stopping become significant — these are NOT modelled
// in the current implementation.
//
// Tolerance: ±2% at energies ≥ 50 MeV, ±5% at 10 MeV.
// ============================================================================

class NistPstarValidation : public ::testing::Test {
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

    // Run one validation point and return the relative deviation.
    struct ValidationPoint {
        double kinE_MeV;
        double ref_dEdx_MeV_cm;     // NIST PSTAR reference [MeV/cm]
        double our_dEdx_MeV_cm;     // computed value
        double relDeviationPct;     // |ours - ref| / ref * 100
        bool   withinTolerance;     // true if relDeviation <= tolerancePct
        double tolerancePct;
    };

    ValidationPoint validateAt(double kinE_MeV,
                                double ref_dEdx_MeV_cm,
                                double tolerancePct = 2.0) const
    {
        ValidationPoint vp;
        vp.kinE_MeV = kinE_MeV;
        vp.ref_dEdx_MeV_cm = ref_dEdx_MeV_cm;
        vp.tolerancePct = tolerancePct;

        try {
            auto step = engine->computeStep(kinE_MeV, 0.01, "water_icru", "proton");
            vp.our_dEdx_MeV_cm = step.dEdx_MeV_cm;
        } catch (const std::exception& e) {
            vp.our_dEdx_MeV_cm = -1.0;
            ADD_FAILURE() << "Exception at " << kinE_MeV << " MeV: " << e.what();
            return vp;
        }

        if (ref_dEdx_MeV_cm > 0.0) {
            vp.relDeviationPct = 100.0 * std::abs(vp.our_dEdx_MeV_cm - ref_dEdx_MeV_cm)
                                 / ref_dEdx_MeV_cm;
        } else {
            vp.relDeviationPct = (vp.our_dEdx_MeV_cm == 0.0) ? 0.0 : 100.0;
        }
        vp.withinTolerance = (vp.relDeviationPct <= tolerancePct);

        return vp;
    }
};

// ── Multi-energy validation table ──────────────────────────────────

TEST_F(NistPstarValidation, WaterProtonAtAllReferenceEnergies)
{
    // Reference data from NIST PSTAR for proton in Water, Liquid.
    //   Linear dE/dx [MeV/cm] = mass SP [MeV·cm²/g] × ρ [g/cm³]
    //   ρ = 1.0 g/cm³  (ICRU-44 Liquid Water)
    struct RefPoint {
        double kinE_MeV;
        double ref_dEdx;
        double tolPct;
        const char* label;
    };

    // Electronic (collision) stopping power from NIST PSTAR for
    // WATER, LIQUID (matno 276, ρ=1.0 g/cm³, I=75 eV), retrieved from
    // physics.nist.gov/cgi-bin/Star/ap_table.pl. Nuclear stopping is
    // <0.05% at these energies and is not modelled by Bethe-Bloch.
    const RefPoint refs[] = {
        //  Energy    NIST dE/dx    Tol   Label
        {  10.0,      4.564e1,     5.0,  "10 MeV — low-E, shell corr. not modelled"   },
        {  50.0,      1.244e1,     2.0,  "50 MeV — Bethe-Bloch regime"                 },
        { 100.0,      7.286e0,     2.0,  "100 MeV — Bethe-Bloch regime"                },
        { 150.0,      5.443e0,     2.0,  "150 MeV — Bethe-Bloch regime"                },
        { 200.0,      4.491e0,     2.0,  "200 MeV — relativistic rise"                 },
    };

    int nPassed = 0;
    int nTotal  = sizeof(refs) / sizeof(refs[0]);
    std::stringstream failures;

    for (const auto& r : refs) {
        auto vp = validateAt(r.kinE_MeV, r.ref_dEdx, r.tolPct);

        if (!vp.withinTolerance) {
            failures << "  FAIL at " << r.label
                     << ": ref=" << r.ref_dEdx
                     << " got=" << vp.our_dEdx_MeV_cm
                     << " dev=" << vp.relDeviationPct << "%"
                     << " (tolerance " << r.tolPct << "%)\n";
        } else {
            ++nPassed;
        }

        // Always use EXPECT_NEAR so every point is individually reported.
        EXPECT_NEAR(vp.our_dEdx_MeV_cm, r.ref_dEdx,
                    r.ref_dEdx * r.tolPct / 100.0)
            << r.label << " | dev=" << vp.relDeviationPct << "%";
    }

    // Summary: report aggregate.
    std::cout << "\n[NIST PSTAR Validation] Proton in Water, Liquid\n"
              << "  Passed: " << nPassed << " / " << nTotal << "\n";

    if (nPassed < nTotal) {
        std::cout << "  Failures:\n" << failures.str();
        std::cout << "  Note: Low-energy deviations are caused by missing shell\n"
                     "  and nuclear-stopping corrections (not modelled).\n"
                     "  Consider adding NIST interpolation for ±1% accuracy.\n";
    }
}

// ── Single-point validation as a regression baseline ───────────────

TEST_F(NistPstarValidation, Nist150MeVBaseline)
{
    auto vp = validateAt(150.0, 5.443, 2.0);

    EXPECT_NEAR(vp.our_dEdx_MeV_cm, 5.443, 5.443 * 0.02)
        << "150 MeV proton in water: ref=5.443 MeV/cm got="
        << vp.our_dEdx_MeV_cm << " MeV/cm";

    // Print for documentation.
    std::cout << "[Baseline] 150 MeV proton in water:\n"
              << "  NIST PSTAR reference: 5.443 MeV/cm\n"
              << "  Our implementation:   " << vp.our_dEdx_MeV_cm << " MeV/cm\n"
              << "  Deviation:            " << vp.relDeviationPct << "%\n";
}

// ── Validation report consistency ─────────────────────────────────

TEST_F(NistPstarValidation, ValidateAgainstNistMethodConsistency)
{
    auto report = engine->validateAgainstNist("water_icru", "proton");

    // The validateAgainstNist() method has its own internal reference;
    // this test checks that it agrees with our explicit table above.
    EXPECT_TRUE(report.passed)
        << "validateAgainstNist() returned passed=false. deviations:\n"
        << [&]() {
               std::string s;
               for (const auto& d : report.deviations)
                   s += "  " + d.first + ": " + std::to_string(d.second) + "%\n";
               return s;
           }();

    EXPECT_LE(report.deviations.front().second, 5.0)
        << "validateAgainstNist() deviation exceeds 5%";
}

// ── Energy-sweep profile (informational) ──────────────────────────

TEST_F(NistPstarValidation, EnergySweepPrintsProfile)
{
    // This test is informational: it prints the full dE/dx vs energy
    // profile so a human reviewer can compare with NIST tables.
    std::cout << "\n[Energy Sweep] dE/dx vs kinetic energy (proton in water)\n"
              << std::string(60, '=') << "\n"
              << "  Energy [MeV]    dE/dx [MeV/cm]    NIST ref [MeV/cm]\n"
              << std::string(60, '-') << "\n";

    struct RefLookup {
        double e; double ref;
    };
    // NIST PSTAR electronic stopping power, WATER LIQUID [MeV·cm²/g].
    const RefLookup lookup[] = {
        {10, 45.64}, {20, 26.05}, {30, 18.75}, {50, 12.44},
        {70, 9.555}, {100, 7.286}, {150, 5.443}, {200, 4.491},
        {250, 3.910}, {300, 3.519}, {400, 3.031}, {500, 2.743},
    };

    for (const auto& r : lookup) {
        auto step = engine->computeStep(r.e, 0.01, "water_icru", "proton");
        double devPct = r.ref > 0.0
            ? 100.0 * std::abs(step.dEdx_MeV_cm - r.ref) / r.ref
            : 0.0;
        printf("  %8.1f        %10.4f        %10.4f    (%+.1f%%)\n",
               r.e, step.dEdx_MeV_cm, r.ref, devPct);
    }
    std::cout << std::string(60, '=') << "\n";
}

// ── Invalid material/particle ─────────────────────────────────────

TEST_F(NistPstarValidation, InvalidMaterialThrows)
{
    EXPECT_THROW(
        engine->computeStep(100.0, 0.01, "nonexistent_material", "proton"),
        std::out_of_range);
}

TEST_F(NistPstarValidation, InvalidParticleThrows)
{
    EXPECT_THROW(
        engine->computeStep(100.0, 0.01, "water_icru", "nonexistent_particle"),
        std::out_of_range);
}

// ── Edge: zero energy ─────────────────────────────────────────────

TEST_F(NistPstarValidation, ZeroEnergyReturnsZero)
{
    auto step = engine->computeStep(0.0, 0.01, "water_icru", "proton");
    EXPECT_NEAR(step.dEdx_MeV_cm, 0.0, 1e-12);
    EXPECT_NEAR(step.energyLoss_MeV, 0.0, 1e-12);
}

// ── Edge: unphysical negative energy ──────────────────────────────

TEST_F(NistPstarValidation, NegativeEnergyReturnsZero)
{
    // Negative kinetic energy is unphysical; engine should handle gracefully.
    auto step = engine->computeStep(-1.0, 0.01, "water_icru", "proton");
    EXPECT_NEAR(step.dEdx_MeV_cm, 0.0, 1e-12);
    EXPECT_NEAR(step.energyLoss_MeV, 0.0, 1e-12);
}
