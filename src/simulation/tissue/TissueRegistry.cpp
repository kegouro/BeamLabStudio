#include "simulation/tissue/TissueRegistry.h"

#include <algorithm>
#include <cctype>

namespace beamlab::simulation {

// ICRU-44 / NIST values used throughout.
// I_eV (mean excitation energy): ICRU Report 49 Table 2.1
// density: typical in-vivo values.

TissueMaterial TissueRegistry::water()
{
    return {"Water", "H2O", 1.000, 7.22, 12.01, 75.0, 36.08};
}

TissueMaterial TissueRegistry::muscle()
{
    // Skeletal muscle (ICRU-44 composition)
    return {"Skeletal Muscle", "muscle", 1.050, 7.64, 13.19, 74.7, 35.57};
}

TissueMaterial TissueRegistry::corticalBone()
{
    // Cortical bone (ICRU-44)
    return {"Cortical Bone", "bone", 1.920, 13.80, 22.39, 106.4, 17.59};
}

TissueMaterial TissueRegistry::fat()
{
    // Adipose tissue (ICRU-44)
    return {"Adipose Tissue", "fat", 0.950, 6.46, 11.00, 63.2, 38.85};
}

TissueMaterial TissueRegistry::brain()
{
    // Brain (grey + white matter average, ICRU-44)
    return {"Brain", "brain", 1.040, 7.57, 13.01, 73.9, 36.01};
}

TissueMaterial TissueRegistry::lung()
{
    // Lung (inflated, ICRU-44)
    return {"Lung (inflated)", "lung", 0.260, 7.64, 13.19, 74.9, 35.57};
}

TissueMaterial TissueRegistry::air()
{
    // Dry air at STP
    return {"Air (dry)", "air", 0.001205, 7.36, 14.51, 85.7, 30420.0};
}

TissueRegistry::TissueRegistry()
{
    materials_ = {
        water(),
        muscle(),
        corticalBone(),
        fat(),
        brain(),
        lung(),
        air()
    };
}

const std::vector<TissueMaterial>& TissueRegistry::all() const
{
    return materials_;
}

std::optional<TissueMaterial> TissueRegistry::find(const std::string& symbol) const
{
    const auto lower = [](std::string s) {
        for (auto& c : s) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        return s;
    };

    const auto target = lower(symbol);

    for (const auto& mat : materials_) {
        if (lower(mat.symbol) == target || lower(mat.name) == target) {
            return mat;
        }
    }

    return std::nullopt;
}

} // namespace beamlab::simulation
