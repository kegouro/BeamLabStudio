#pragma once

// NistValidator — headless comparator for C4 (NIST Validation Panel).
//
// Compares stopping-power output from two engines:
//   (1) domain::SimulationEngine  (Bethe-Bloch via MaterialRegistry/ParticleRegistry)
//   (2) biosim::StoppingPowerEngine (Bethe-Bloch via BioMaterialLibrary/ParticleLibrary)
//
// If (particle, material) == ("proton", "water") the comparison includes a
// REAL external NIST PSTAR reference read from the project fixture file.
// For any other combination the comparison is inter-engine only — NO fabricated
// reference is injected.
//
// Unit convention (CAREFUL):
//   NIST PSTAR gives  mass stopping power  [MeV·cm²/g].
//   SimulationEngine::computeStep() returns dEdx_MeV_cm  [MeV/cm]  (linear).
//   StoppingPowerEngine::dEdx_MeV_cm() also returns [MeV/cm] (linear).
//   StoppingPowerEngine::massStoppingPower() returns [MeV·cm²/g].
//
//   Conversion:  mass_SP [MeV·cm²/g] = linear_dEdx [MeV/cm] / density [g/cm³]
//   (applies to both engines when building the NIST column).
//
// ponytail: pure C++20, no Qt — links directly into headless test binary.

#include "biosim/materials/BioMaterialLibrary.h"
#include "biosim/physics/ParticleLibrary.h"
#include "biosim/physics/StoppingPowerEngine.h"
#include "domain/materials/MaterialRegistry.h"
#include "domain/physics/ParticleRegistry.h"
#include "domain/simulation/SimulationEngine.h"

#include <optional>
#include <string>
#include <vector>

namespace beamlab::ui::validation {

// ── Data structures ─────────────────────────────────────────────────────────

/// One row in the comparison table.
struct ValidationRow {
    double energy_MeV{0.0};

    // Domain engine: linear dE/dx [MeV/cm]; empty string "" if engine unavailable.
    double domain_dEdx_MeV_cm{0.0};
    bool   domain_available{false};

    // Biosim engine: linear dE/dx [MeV/cm]; unavailable if material/particle
    // not found in biosim catalogs.
    double biosim_dEdx_MeV_cm{0.0};
    bool   biosim_available{false};

    // NIST reference: mass stopping power [MeV·cm²/g] converted to linear
    // [MeV/cm] via density. Only populated when hasNistReference == true.
    double nist_dEdx_MeV_cm{0.0};
    bool   has_nist{false};

    // Deviations [%] relative to NIST (only valid when has_nist == true).
    double dev_domain_pct{0.0};
    double dev_biosim_pct{0.0};

    // Inter-engine deviation [%] relative to domain (when nist absent).
    double dev_interengine_pct{0.0};

    // Pass/fail per engine vs NIST threshold (5 %).
    bool domain_pass{false};
    bool biosim_pass{false};
};

/// Result of a full validation run.
struct ValidationResult {
    bool hasNistReference{false};   // true only for proton/water

    std::string particleId{};       // e.g. "proton"
    std::string materialId{};       // e.g. "water_icru" (domain) or "water_icru" (biosim)

    std::vector<ValidationRow> rows{};

    // Summary statistics (over rows where data available).
    double maxDevDomain_pct{0.0};
    double maxDevBiosim_pct{0.0};
    double maxDevInterengine_pct{0.0};

    // Note to display in the UI footer.
    std::string note{};
};

// ── Validator class ──────────────────────────────────────────────────────────

/// Builds a ValidationResult for (domainParticle, domainMaterial).
///
/// - domainMaterialId / domainParticleId : keys for MaterialRegistry /
///   ParticleRegistry (domain layer).
/// - biosimMaterialId / biosimParticleId : keys for BioMaterialLibrary /
///   ParticleLibrary (biosim layer). Pass empty string to skip biosim column.
/// - energies_MeV : list of energies to evaluate; default uses NIST fixture
///   grid when hasNist, or a clinical band otherwise.
class NistValidator {
public:
    struct Options {
        std::string domainMaterialId{"water_icru"};
        std::string domainParticleId{"proton"};

        std::string biosimMaterialId{"water_icru"};  // id in BioMaterialLibrary
        std::string biosimParticleId{"proton"};      // id in ParticleLibrary

        // Path to the NIST fixture directory (PROJECT_SOURCE_DIR/tests/fixtures).
        // If empty the validator tries to locate it relative to the executable.
        std::string fixtureDir{};

        // Energy grid to evaluate [MeV]. If empty:
        //   • hasNist → use NIST fixture energies
        //   • otherwise → default clinical grid 10–300 MeV
        std::vector<double> energies_MeV{};

        static constexpr double k_nist_threshold_pct = 5.0;
    };

    explicit NistValidator(
        const domain::materials::MaterialRegistry& matReg,
        const domain::physics::ParticleRegistry& partReg);

    /// Run validation. Never throws; errors are reflected in the result rows.
    [[nodiscard]] ValidationResult validate(const Options& opts) const;

private:
    // Load NIST PSTAR proton/water fixture. Returns empty vector on failure.
    struct NistRow {
        double energy_MeV{0.0};
        double massSP_MeV_cm2_g{0.0};
    };
    static std::vector<NistRow> loadNistFixture(const std::string& fixtureDir);

    // Default clinical energy grid (MeV).
    static std::vector<double> defaultEnergyGrid();

    const domain::materials::MaterialRegistry& matReg_;
    const domain::physics::ParticleRegistry&   partReg_;
};

} // namespace beamlab::ui::validation
