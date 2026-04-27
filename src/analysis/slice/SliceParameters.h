#pragma once

#include "analysis/slice/SliceMode.h"

#include <cstddef>

namespace beamlab::analysis {

struct SliceParameters {
    SliceMode mode{SliceMode::ByFrameIndex};

    std::size_t half_window_frames{5};
    std::size_t max_slices{11};
};

} // namespace beamlab::analysis
