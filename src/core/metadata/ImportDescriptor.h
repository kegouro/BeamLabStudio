#pragma once

#include <string>

namespace beamlab::core {

struct ImportDescriptor {
    std::string importer_name{};
    std::string importer_version{};
    std::string schema_name{};
};

} // namespace beamlab::core
