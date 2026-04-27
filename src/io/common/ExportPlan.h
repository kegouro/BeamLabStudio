#pragma once

#include <string>

namespace beamlab::io {

struct ExportPlan {
    std::string target_path{};
    std::string format_name{};
};

} // namespace beamlab::io
