#include "biosim/materials/BioMaterialValidator.h"

#include <cmath>
#include <sstream>

namespace beamlab::biosim {

// ── Helpers ───────────────────────────────────────────────────────────────────

void BioMaterialValidator::addError(MaterialValidationReport& r,
                                    const std::string& field,
                                    const std::string& text)
{
    r.messages.push_back({ValidationMessage::Level::Error, field, text});
    r.has_errors = true;
}

void BioMaterialValidator::addWarning(MaterialValidationReport& r,
                                      const std::string& field,
                                      const std::string& text)
{
    r.messages.push_back({ValidationMessage::Level::Warning, field, text});
}

void BioMaterialValidator::addInfo(MaterialValidationReport& r,
                                   const std::string& field,
                                   const std::string& text)
{
    r.messages.push_back({ValidationMessage::Level::Info, field, text});
}

// ── Individual checks ─────────────────────────────────────────────────────────

void BioMaterialValidator::checkIdentification(const BioMaterial& mat,
                                               MaterialValidationReport& r) const
{
    if (mat.id.empty()) {
        addError(r, "id", "Material ID must not be empty.");
    }
    if (mat.name.empty()) {
        addWarning(r, "name", "Material name is empty — will show as unnamed in UI.");
    }
}

void BioMaterialValidator::checkStoppingPower(const BioMaterial& mat,
                                              MaterialValidationReport& r) const
{
    if (mat.density_g_cm3 <= 0.0) {
        addError(r, "density_g_cm3",
                 "Density must be > 0 g/cm³. Got: " +
                 std::to_string(mat.density_g_cm3));
    }

    if (mat.Z_eff < 1.0) {
        addError(r, "Z_eff",
                 "Effective atomic number Z_eff must be ≥ 1. Got: " +
                 std::to_string(mat.Z_eff));
    }

    if (mat.A_eff < mat.Z_eff) {
        addError(r, "A_eff",
                 "Effective atomic mass A_eff must be ≥ Z_eff. Got A=" +
                 std::to_string(mat.A_eff) + " < Z=" + std::to_string(mat.Z_eff));
    }

    if (mat.I_eV < 10.0) {
        addError(r, "I_eV",
                 "Mean excitation energy I must be ≥ 10 eV (physical limit). Got: " +
                 std::to_string(mat.I_eV) + " eV");
    } else {
        // Empirical rule: I ≈ 13.5 · Z_eff [eV] (within a factor of 2 is normal)
        const double I_empirical = 13.5 * mat.Z_eff;
        const double ratio = mat.I_eV / I_empirical;
        if (ratio < 0.5 || ratio > 2.0) {
            std::ostringstream ss;
            ss << "I_eV = " << mat.I_eV << " eV deviates significantly from the "
               << "empirical rule I ≈ 13.5·Z_eff = " << I_empirical
               << " eV (ratio = " << ratio << "). "
               << "Verify against ICRU-49 table for this material.";
            addWarning(r, "I_eV", ss.str());
        }
    }

    if (mat.radiation_length_cm <= 0.0) {
        addWarning(r, "radiation_length_cm",
                   "Radiation length X₀ must be > 0 cm. "
                   "MCS calculations will be skipped.");
    }
}

void BioMaterialValidator::checkSternheimer(const BioMaterial& mat,
                                            MaterialValidationReport& r) const
{
    if (!mat.has_sternheimer) {
        addInfo(r, "has_sternheimer",
                "Sternheimer density-effect correction disabled. "
                "Bethe-Bloch will overestimate stopping power at high βγ (>10). "
                "Results are conservative.");
        return;
    }

    if (mat.sternheimer_x0 >= mat.sternheimer_x1) {
        addWarning(r, "sternheimer_x0/x1",
                   "Sternheimer parameter x₀ ≥ x₁ (" +
                   std::to_string(mat.sternheimer_x0) + " ≥ " +
                   std::to_string(mat.sternheimer_x1) + "). "
                   "Density correction will be clipped to zero.");
    }

    if (mat.sternheimer_k < 2.0 || mat.sternheimer_k > 5.0) {
        addWarning(r, "sternheimer_k",
                   "Sternheimer exponent k = " + std::to_string(mat.sternheimer_k) +
                   " is outside the typical range [2, 5].");
    }
}

void BioMaterialValidator::checkBiological(const BioMaterial& mat,
                                           MaterialValidationReport& r) const
{
    if (mat.WR < 1.0 || mat.WR > 20.0) {
        addWarning(r, "WR",
                   "Radiation weighting factor WR = " + std::to_string(mat.WR) +
                   " is outside ICRP-103 defined range [1, 20]. "
                   "Verify for your particle type.");
    }

    if (mat.WT < 0.0 || mat.WT > 1.0) {
        addWarning(r, "WT",
                   "Tissue weighting factor WT = " + std::to_string(mat.WT) +
                   " must be in [0, 1] (ICRP-103).");
    }

    if (mat.alpha_beta_ratio < 0.5 || mat.alpha_beta_ratio > 20.0) {
        addWarning(r, "alpha_beta_ratio",
                   "α/β ratio = " + std::to_string(mat.alpha_beta_ratio) +
                   " Gy is outside the plausible biological range [0.5, 20] Gy. "
                   "Typical values: tumor ≈ 10 Gy, late-CNS ≈ 2 Gy.");
    }
}

void BioMaterialValidator::checkComposition(const BioMaterial& mat,
                                            MaterialValidationReport& r) const
{
    if (mat.composition.empty()) {
        addInfo(r, "composition",
                "No elemental composition defined. "
                "Z_eff and A_eff will be used directly for stopping-power calculations.");
        return;
    }

    double sum = 0.0;
    for (const auto& el : mat.composition) {
        if (el.fraction < 0.0) {
            addError(r, "composition",
                     "Element " + el.symbol + " has negative mass fraction: " +
                     std::to_string(el.fraction));
        }
        sum += el.fraction;
    }

    if (std::abs(sum - 1.0) > 0.001) {
        std::ostringstream ss;
        ss << "Elemental mass fractions sum to " << sum
           << " instead of 1.0 (difference: " << (sum - 1.0) << "). "
           << "Normalize the composition for accurate Z/A calculations.";
        addWarning(r, "composition", ss.str());
    }
}

void BioMaterialValidator::checkOptical(const BioMaterial& mat,
                                        MaterialValidationReport& r) const
{
    if (mat.scintillation_yield_photons_MeV > 0.0 && mat.refraction_index <= 1.0) {
        addInfo(r, "refraction_index",
                "Scintillation yield is set but refraction index = 1.0. "
                "Set the correct refractive index for optical photon simulations.");
    }
}

void BioMaterialValidator::checkDensityForCategory(const BioMaterial& mat,
                                                   MaterialValidationReport& r) const
{
    const auto warn = [&](const std::string& msg) {
        addWarning(r, "density_g_cm3", msg);
    };

    const double rho = mat.density_g_cm3;

    if (mat.category == "biological") {
        if (rho < 0.05 || rho > 3.0) {
            warn("Density " + std::to_string(rho) +
                 " g/cm³ is unusual for a biological material (expected 0.05–3.0 g/cm³). "
                 "Verify that this is not a unit error.");
        }
    } else if (mat.category == "gas") {
        if (rho > 0.05) {
            warn("Density " + std::to_string(rho) +
                 " g/cm³ is unusually high for a gas (expected < 0.05 g/cm³). "
                 "Did you mean kg/m³?");
        }
    } else if (mat.category == "metal" || mat.category == "shielding") {
        if (rho < 1.0) {
            warn("Density " + std::to_string(rho) +
                 " g/cm³ is unusually low for a metal/shielding material "
                 "(expected > 1.0 g/cm³).");
        }
    }
}

// ── Main entry point ──────────────────────────────────────────────────────────

MaterialValidationReport BioMaterialValidator::validate(const BioMaterial& mat) const
{
    MaterialValidationReport report;
    checkIdentification(mat, report);
    checkStoppingPower(mat, report);
    checkSternheimer(mat, report);
    checkBiological(mat, report);
    checkComposition(mat, report);
    checkOptical(mat, report);
    checkDensityForCategory(mat, report);
    return report;
}

} // namespace beamlab::biosim
