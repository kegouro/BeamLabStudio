#pragma once

#include "biosim/geometry/BioSlab.h"

#include <string>
#include <vector>

namespace beamlab::biosim {

struct PhantomPreset {
    std::string id{};
    std::string name{};
    std::string category{};      // "IAEA" | "ICRU" | "Clinical" | "Experimental"
    std::string description{};
    std::string reference{};
    std::vector<BioSlab> slabs{};
};

} // namespace beamlab::biosim
