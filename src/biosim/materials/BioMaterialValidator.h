#pragma once

#include "biosim/materials/BioMaterial.h"

#include <string>
#include <vector>

namespace beamlab::biosim {

struct ValidationMessage {
    enum class Level { Info, Warning, Error };
    Level level{Level::Info};
    std::string field{};    // which field triggered this message
    std::string text{};     // human-readable explanation
};

struct MaterialValidationReport {
    std::vector<ValidationMessage> messages{};
    bool has_errors{false};

    [[nodiscard]] bool isValid() const { return !has_errors; }

    [[nodiscard]] std::vector<ValidationMessage> errors() const
    {
        std::vector<ValidationMessage> out;
        for (const auto& m : messages) {
            if (m.level == ValidationMessage::Level::Error) out.push_back(m);
        }
        return out;
    }

    [[nodiscard]] std::vector<ValidationMessage> warnings() const
    {
        std::vector<ValidationMessage> out;
        for (const auto& m : messages) {
            if (m.level == ValidationMessage::Level::Warning) out.push_back(m);
        }
        return out;
    }
};

// Validates physical coherence of a BioMaterial.
//
// ERRORs (material cannot be used):
//   - density ≤ 0
//   - I_eV < 10 eV  (physically impossible: I ≥ Z·10 eV empirically)
//   - Z_eff < 1
//   - A_eff < Z_eff  (A must be ≥ Z by definition)
//   - id is empty
//
// WARNINGs (material can be used but results may be inaccurate):
//   - I_eV deviates by >50% from empirical rule I ≈ 13.5·Z_eff [eV]
//   - Sum of elemental fractions differs from 1.0 by >0.001
//   - Sternheimer x0 ≥ x1
//   - density unusually high/low for declared category
//   - WR outside known range [1, 20]
//   - alpha_beta_ratio outside plausible range [0.5, 20] Gy
//
// INFOs (purely informational):
//   - has_sternheimer = false (density correction disabled)
//   - no elemental composition defined
//   - scintillation yield > 0 but refraction_index = 1.0
class BioMaterialValidator {
public:
    [[nodiscard]] MaterialValidationReport validate(const BioMaterial& mat) const;

private:
    void checkIdentification(const BioMaterial& mat, MaterialValidationReport& r) const;
    void checkStoppingPower(const BioMaterial& mat, MaterialValidationReport& r) const;
    void checkSternheimer(const BioMaterial& mat, MaterialValidationReport& r) const;
    void checkBiological(const BioMaterial& mat, MaterialValidationReport& r) const;
    void checkComposition(const BioMaterial& mat, MaterialValidationReport& r) const;
    void checkOptical(const BioMaterial& mat, MaterialValidationReport& r) const;
    void checkDensityForCategory(const BioMaterial& mat, MaterialValidationReport& r) const;

    static void addError(MaterialValidationReport& r,
                         const std::string& field, const std::string& text);
    static void addWarning(MaterialValidationReport& r,
                           const std::string& field, const std::string& text);
    static void addInfo(MaterialValidationReport& r,
                        const std::string& field, const std::string& text);
};

} // namespace beamlab::biosim
