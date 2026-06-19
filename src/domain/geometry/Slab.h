#pragma once

// Re-export of the existing BioSlab under the new domain::geometry namespace.
// After full migration, this will contain the actual Slab struct definition.

#include "biosim/geometry/BioSlab.h"

namespace beamlab::domain::geometry {

using Slab = beamlab::biosim::BioSlab;

} // namespace beamlab::domain::geometry
