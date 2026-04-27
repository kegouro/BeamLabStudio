#pragma once

namespace beamlab::core {

enum class StatusCode {
    Ok = 0,
    UnknownError,
    InvalidArgument,
    InvalidState,
    NotFound,
    NotImplemented,
    ParseError,
    ValidationError,
    IOError
};

} // namespace beamlab::core
