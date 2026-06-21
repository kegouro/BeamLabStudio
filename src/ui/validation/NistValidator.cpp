#include "ui/validation/NistValidator.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace beamlab::ui::validation {

// ── Static helpers ──────────────────────────────────────────────────────────

namespace {

// Canonical fixture filename.
constexpr const char* k_fixture_filename = "nist_pstar_proton_water.csv";

// Returns true when the combination is the one for which we have a REAL
// external reference. Checks both domain and biosim IDs.
bool isProtonWaterCombination(const NistValidator::Options& opts)
{
    const bool domainMatch =
        (opts.domainParticleId == "proton") &&
        (opts.domainMaterialId == "water_icru" ||
         opts.domainMaterialId == "water");
    const bool biosimMatch =
        opts.biosimParticleId.empty() ||
        (opts.biosimParticleId == "proton" &&
         (opts.biosimMaterialId == "water_icru" ||
          opts.biosimMaterialId == "water"));
    return domainMatch && biosimMatch;
}

} // namespace

// ── Constructor ─────────────────────────────────────────────────────────────

NistValidator::NistValidator(
    const domain::materials::MaterialRegistry& matReg,
    const domain::physics::ParticleRegistry&   partReg)
    : matReg_(matReg), partReg_(partReg)
{
}

// ── NIST fixture loader ──────────────────────────────────────────────────────

std::vector<NistValidator::NistRow>
NistValidator::loadNistFixture(const std::string& fixtureDir)
{
    // Try the provided directory first, then fallback paths.
    std::vector<std::string> candidates;
    if (!fixtureDir.empty())
        candidates.push_back(fixtureDir + "/" + k_fixture_filename);

#ifdef PROJECT_SOURCE_DIR
    candidates.push_back(
        std::string(PROJECT_SOURCE_DIR) + "/tests/fixtures/" + k_fixture_filename);
#endif

    std::ifstream in;
    for (const auto& path : candidates) {
        in.open(path);
        if (in.is_open()) break;
    }
    if (!in.is_open()) return {};

    std::vector<NistRow> rows;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::stringstream ss(line);
        std::string eStr, spStr;
        if (!std::getline(ss, eStr, ',')) continue;
        if (!std::getline(ss, spStr, ',')) continue;
        try {
            rows.push_back({std::stod(eStr), std::stod(spStr)});
        } catch (...) { /* skip malformed rows */ }
    }
    return rows;
}

// ── Default energy grid ─────────────────────────────────────────────────────

std::vector<double> NistValidator::defaultEnergyGrid()
{
    // Clinical band with coarse sampling, suitable for inter-engine display.
    return {10.0, 20.0, 30.0, 40.0, 50.0, 70.0, 100.0, 125.0,
            150.0, 175.0, 200.0, 225.0, 250.0};
}

// ── validate() ──────────────────────────────────────────────────────────────

ValidationResult NistValidator::validate(const Options& opts) const
{
    ValidationResult result;
    result.particleId  = opts.domainParticleId;
    result.materialId  = opts.domainMaterialId;

    const bool hasNist = isProtonWaterCombination(opts);
    result.hasNistReference = hasNist;

    // ── Resolve domain material/particle ───────────────────────────────────
    auto domainMatOpt  = matReg_.find(opts.domainMaterialId);
    auto domainPartOpt = partReg_.find(opts.domainParticleId);

    const bool domainOk = domainMatOpt.has_value() && domainPartOpt.has_value();

    // density [g/cm³] for linear ↔ mass SP conversion.
    // ponytail: mass_SP [MeV·cm²/g] = linear_dEdx [MeV/cm] / density [g/cm³]
    double domainDensity = 1.0;
    std::unique_ptr<domain::simulation::SimulationEngine> domainEngine;
    if (domainOk) {
        domainDensity = domainMatOpt->get().density_g_cm3;
        domainEngine  = std::make_unique<domain::simulation::SimulationEngine>(
            matReg_, partReg_);
    }

    // ── Resolve biosim material/particle ───────────────────────────────────
    biosim::BioMaterialLibrary biosimMatLib;
    biosim::ParticleLibrary    biosimPartLib;
    biosim::StoppingPowerEngine biosimEngine;

    const bool wantBiosim = !opts.biosimMaterialId.empty() &&
                             !opts.biosimParticleId.empty();
    std::optional<biosim::BioMaterial>      biosimMat;
    std::optional<biosim::ParticleSpecies>  biosimPart;
    if (wantBiosim) {
        biosimMat  = biosimMatLib.findById(opts.biosimMaterialId);
        biosimPart = biosimPartLib.findById(opts.biosimParticleId);
    }
    const bool biosimOk = biosimMat.has_value() && biosimPart.has_value();

    // ── Energy grid ─────────────────────────────────────────────────────────
    std::vector<double> energies = opts.energies_MeV;

    if (hasNist && energies.empty()) {
        // Use NIST fixture energies as the authoritative grid.
        auto nistRows = loadNistFixture(opts.fixtureDir);
        for (const auto& nr : nistRows)
            energies.push_back(nr.energy_MeV);
    }
    if (energies.empty())
        energies = defaultEnergyGrid();

    // ── Load NIST reference (keyed by energy for fast lookup) ───────────────
    struct NistLookup {
        double energy_MeV{0.0};
        double massSP_MeV_cm2_g{0.0};
    };
    std::vector<NistLookup> nistLookup;
    if (hasNist) {
        auto nistRows = loadNistFixture(opts.fixtureDir);
        for (const auto& nr : nistRows)
            nistLookup.push_back({nr.energy_MeV, nr.massSP_MeV_cm2_g});
    }

    auto findNist = [&](double E) -> std::optional<double> {
        if (nistLookup.empty()) return std::nullopt;
        // Exact match (fixture energies used as grid, so this should always hit).
        for (const auto& nl : nistLookup) {
            if (std::abs(nl.energy_MeV - E) < 0.01)
                return nl.massSP_MeV_cm2_g;
        }
        return std::nullopt;
    };

    // ── Build rows ───────────────────────────────────────────────────────────
    // Summary maxima are restricted to the validated clinical band (50-250 MeV)
    // where Bethe-Bloch without shell corrections is accurate. Low-energy rows
    // (< 50 MeV) are shown in the table for completeness but excluded from the
    // summary stat — Bethe-Bloch loses accuracy there without shell corrections.
    // ponytail: matches existing domain/biosim NIST regression test band.
    constexpr double k_summary_lo_MeV = 50.0;
    constexpr double k_summary_hi_MeV = 250.0;
    double maxDevDomain      = 0.0;
    double maxDevBiosim      = 0.0;
    double maxDevInterengine = 0.0;

    for (double E : energies) {
        ValidationRow row;
        row.energy_MeV = E;

        // Domain dE/dx [MeV/cm] (linear).
        if (domainOk && domainEngine) {
            try {
                auto step = domainEngine->computeStep(
                    E, 0.01,  // thin step: 0.01 cm
                    opts.domainMaterialId,
                    opts.domainParticleId);
                row.domain_dEdx_MeV_cm = step.dEdx_MeV_cm;
                row.domain_available   = true;
            } catch (...) {
                row.domain_available = false;
            }
        }

        // Biosim dE/dx [MeV/cm] (linear).
        if (biosimOk) {
            try {
                row.biosim_dEdx_MeV_cm = biosimEngine.dEdx_MeV_cm(
                    E, *biosimMat, *biosimPart);
                row.biosim_available = true;
            } catch (...) {
                row.biosim_available = false;
            }
        }

        // NIST reference.
        auto nistMassSP = findNist(E);
        if (hasNist && nistMassSP.has_value()) {
            // Convert mass SP → linear using domain material density.
            // ponytail: linear_dEdx = mass_SP × density
            const double rho = domainOk ? domainDensity : 1.0;
            row.nist_dEdx_MeV_cm = (*nistMassSP) * rho;
            row.has_nist         = true;

            if (row.domain_available && row.nist_dEdx_MeV_cm > 0.0) {
                row.dev_domain_pct =
                    100.0 * std::abs(row.domain_dEdx_MeV_cm - row.nist_dEdx_MeV_cm) /
                    row.nist_dEdx_MeV_cm;
                row.domain_pass =
                    row.dev_domain_pct <= Options::k_nist_threshold_pct;
                // Accumulate summary only in the clinical band (50-250 MeV).
                if (E >= k_summary_lo_MeV && E <= k_summary_hi_MeV)
                    maxDevDomain = std::max(maxDevDomain, row.dev_domain_pct);
            }
            if (row.biosim_available && row.nist_dEdx_MeV_cm > 0.0) {
                row.dev_biosim_pct =
                    100.0 * std::abs(row.biosim_dEdx_MeV_cm - row.nist_dEdx_MeV_cm) /
                    row.nist_dEdx_MeV_cm;
                row.biosim_pass =
                    row.dev_biosim_pct <= Options::k_nist_threshold_pct;
                if (E >= k_summary_lo_MeV && E <= k_summary_hi_MeV)
                    maxDevBiosim = std::max(maxDevBiosim, row.dev_biosim_pct);
            }
        }

        // Inter-engine deviation (always computed when both available).
        if (row.domain_available && row.biosim_available &&
            row.domain_dEdx_MeV_cm > 0.0) {
            row.dev_interengine_pct =
                100.0 * std::abs(row.domain_dEdx_MeV_cm - row.biosim_dEdx_MeV_cm) /
                row.domain_dEdx_MeV_cm;
            maxDevInterengine = std::max(maxDevInterengine, row.dev_interengine_pct);
        }

        result.rows.push_back(row);
    }

    result.maxDevDomain_pct      = maxDevDomain;
    result.maxDevBiosim_pct      = maxDevBiosim;
    result.maxDevInterengine_pct = maxDevInterengine;

    if (hasNist) {
        result.note = "Validated vs NIST PSTAR (I=75 eV, ICRU-37/49). "
                      "Domain uses ICRU-90 I=78 eV (systematic ~0.4 % shift expected).";
    } else {
        result.note =
            "Sin referencia NIST publicada — comparación inter-motor (consistencia). "
            "No se muestra columna NIST para esta combinación.";
    }

    return result;
}

} // namespace beamlab::ui::validation
