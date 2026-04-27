#pragma once

#include "core/foundation/Severity.h"
#include "core/foundation/StatusCode.h"

#include <string>

namespace beamlab::core {

struct Error {
    StatusCode code{StatusCode::UnknownError};
    Severity severity{Severity::Error};
    std::string message{};
    std::string details{};
};

} // namespace beamlab::core
