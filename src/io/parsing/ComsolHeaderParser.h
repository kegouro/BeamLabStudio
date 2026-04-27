#pragma once

#include <string>
#include <vector>

namespace beamlab::io {

class ComsolHeaderParser {
public:
    [[nodiscard]] static bool looksLikeComsolHeader(const std::string& line);
    [[nodiscard]] static std::vector<std::string> extractTokens(const std::string& line, char delimiter);
};

} // namespace beamlab::io
