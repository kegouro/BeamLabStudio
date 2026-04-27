#pragma once

#include "core/math/AABB.h"

namespace beamlab::data {

struct TrajectorySummary {
    double time_min_s{0.0};
    double time_max_s{0.0};
    beamlab::core::AABB bounds{};
    std::size_t sample_count{0};
};

} // namespace beamlab::data
