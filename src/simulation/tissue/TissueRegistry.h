#pragma once

#include "simulation/tissue/TissueMaterial.h"

#include <optional>
#include <string>
#include <vector>

namespace beamlab::simulation {

// Catalogue of built-in biological materials.
// Values follow ICRU Report 44 / NIST PSTAR database.
class TissueRegistry {
public:
    TissueRegistry();

    // Returns all registered materials.
    [[nodiscard]] const std::vector<TissueMaterial>& all() const;

    // Look up by symbol (case-insensitive). Returns nullopt if not found.
    [[nodiscard]] std::optional<TissueMaterial> find(const std::string& symbol) const;

    // Convenience: water
    [[nodiscard]] static TissueMaterial water();
    [[nodiscard]] static TissueMaterial muscle();
    [[nodiscard]] static TissueMaterial corticalBone();
    [[nodiscard]] static TissueMaterial fat();
    [[nodiscard]] static TissueMaterial brain();
    [[nodiscard]] static TissueMaterial lung();
    [[nodiscard]] static TissueMaterial air();

private:
    std::vector<TissueMaterial> materials_{};
};

} // namespace beamlab::simulation
