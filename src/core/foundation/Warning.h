#pragma once

#include "core/foundation/Severity.h"

#include <string>

namespace beamlab::core {

struct Warning {
    Severity severity{Severity::Warning};
    std::string message{};
    std::string details{};
};

} // namespace beamlab::core
