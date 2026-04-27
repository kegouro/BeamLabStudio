#pragma once

#include <string>
#include <unordered_map>

namespace beamlab::io {

struct ImportMapping {
    std::unordered_map<std::string, std::string> column_to_field{};
};

} // namespace beamlab::io
