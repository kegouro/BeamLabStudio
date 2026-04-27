#pragma once

#include <string>

namespace beamlab::io {

struct ImporterCapabilityScore {
    std::string importer_name{};
    double confidence{0.0};
};

} // namespace beamlab::io
