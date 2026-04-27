#pragma once

#include "core/foundation/Error.h"

#include <optional>
#include <string>

namespace beamlab::io {

struct ExportResult {
    bool success{false};
    std::string output_path{};
    std::optional<beamlab::core::Error> error{};
};

} // namespace beamlab::io
