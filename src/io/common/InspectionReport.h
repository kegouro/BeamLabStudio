#pragma once

#include "core/foundation/Warning.h"

#include <string>
#include <vector>

namespace beamlab::io {

struct InspectionReport {
    std::string file_path{};
    bool readable{false};
    std::vector<std::string> preview_lines{};
    std::vector<beamlab::core::Warning> warnings{};
};

} // namespace beamlab::io
