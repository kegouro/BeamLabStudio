#pragma once

#include <string>

namespace beamlab::io {

class DelimiterDetector {
public:
    [[nodiscard]] static char detect(const std::string& line);
};

} // namespace beamlab::io
