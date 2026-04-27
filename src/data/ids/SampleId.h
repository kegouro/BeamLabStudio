#pragma once

#include "core/foundation/StrongId.h"

namespace beamlab::data {

struct SampleIdTag {};
using SampleId = beamlab::core::StrongId<SampleIdTag>;

} // namespace beamlab::data
