#pragma once

#include "biosim/geometry/PhantomPreset.h"

#include <optional>
#include <string>
#include <vector>

namespace beamlab::biosim {

// Catalogue of 15+ pre-configured phantom stacks.
class PhantomLibrary {
public:
    PhantomLibrary();

    [[nodiscard]] const std::vector<PhantomPreset>& all() const;
    [[nodiscard]] std::optional<PhantomPreset> findById(const std::string& id) const;
    [[nodiscard]] std::vector<PhantomPreset> byCategory(const std::string& cat) const;

private:
    std::vector<PhantomPreset> phantoms_{};

    static PhantomPreset iaeaWater30();
    static PhantomPreset icruHead();
    static PhantomPreset icruTrunk();
    static PhantomPreset protonTherapyHead();
    static PhantomPreset protonTherapyProstate();
    static PhantomPreset randoHead();
    static PhantomPreset generic3Layer();
    static PhantomPreset generic5Layer();
    static PhantomPreset detectorStack();
    static PhantomPreset shieldingTest();
    static PhantomPreset muscleBone();
    static PhantomPreset lungPassage();
    static PhantomPreset brainTreatment();
    static PhantomPreset waterAirInterface();
    static PhantomPreset vacuum();
};

} // namespace beamlab::biosim
