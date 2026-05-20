#pragma once

#include <string>

namespace beamlab::analysis {

class FullEnvelopePreviewBuilder {
public:
    static bool buildPreview(const std::string& csv_path,
                             const std::string& output_obj,
                             std::string* error);
};

} // namespace beamlab::analysis
