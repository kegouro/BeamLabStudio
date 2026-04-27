#pragma once

#include "core/validation/ValidationSeverity.h"

#include <string>

namespace beamlab::core {

struct ValidationIssue {
    ValidationSeverity severity{ValidationSeverity::Info};
    std::string message{};
    std::string details{};
};

} // namespace beamlab::core
