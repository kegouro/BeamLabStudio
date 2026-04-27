#pragma once

#include "core/foundation/Error.h"
#include "core/foundation/Warning.h"
#include "data/model/TrajectoryDataset.h"

#include <optional>
#include <vector>

namespace beamlab::io {

struct ImportResult {
    bool success{false};
    std::optional<beamlab::data::TrajectoryDataset> dataset{};
    std::vector<beamlab::core::Warning> warnings{};
    std::optional<beamlab::core::Error> error{};
};

} // namespace beamlab::io
