#pragma once

#include <string>
#include <vector>

namespace beamlab::io {

class TableHeaderAnalyzer {
public:
    [[nodiscard]] static std::vector<std::string> splitHeader(const std::string& line, char delimiter);
};

} // namespace beamlab::io
