#pragma once

#include <string>

namespace beamlab::core {

struct SourceDescriptor {
    std::string original_path{};
    std::string source_type{};
    std::string display_name{};
};

} // namespace beamlab::core
