#pragma once

#include "core/math/Vec2.h"

#include <cstddef>
#include <vector>

namespace beamlab::analysis {

class ContourResampler {
public:
    [[nodiscard]] std::vector<beamlab::core::Vec2> resampleClosedContour(
        const std::vector<beamlab::core::Vec2>& points,
        std::size_t target_count) const;
};

} // namespace beamlab::analysis
