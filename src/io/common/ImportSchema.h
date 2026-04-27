#pragma once

#include <string>
#include <unordered_map>

namespace beamlab::io {

struct ImportSchema {
    std::string schema_name{"unknown"};
    char delimiter{','};
    std::unordered_map<std::string, std::string> field_mapping{};
};

} // namespace beamlab::io
