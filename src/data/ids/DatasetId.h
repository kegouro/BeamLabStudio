#pragma once

#include "core/foundation/StrongId.h"

namespace beamlab::data {

struct DatasetIdTag {};
using DatasetId = beamlab::core::StrongId<DatasetIdTag>;

} // namespace beamlab::data
