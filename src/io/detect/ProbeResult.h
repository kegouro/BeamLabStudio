#pragma once

#include "io/detect/FormatSignature.h"

#include <string>
#include <vector>

namespace beamlab::io {

struct ProbeResult {
    std::string file_path{};
    std::string extension{};
    std::string first_line_preview{};
    std::vector<std::string> preview_lines{};
    bool readable{false};
    FormatSignature detected_format{FormatSignature::Unknown};
};

} // namespace beamlab::io
