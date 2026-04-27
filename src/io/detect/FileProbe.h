#pragma once

#include "io/detect/ProbeResult.h"

#include <string>

namespace beamlab::io {

class FileProbe {
public:
    [[nodiscard]] ProbeResult probe(const std::string& file_path) const;
};

} // namespace beamlab::io
