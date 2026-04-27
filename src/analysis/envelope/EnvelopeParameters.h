#pragma once

#include "analysis/envelope/EnvelopeMethodType.h"

#include <cstddef>

namespace beamlab::analysis {

struct EnvelopeParameters {
    EnvelopeMethodType method{EnvelopeMethodType::ConvexHull};
    std::size_t angular_bins{128};
    std::size_t minimum_points{3};
};

} // namespace beamlab::analysis
