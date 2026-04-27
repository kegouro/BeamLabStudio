#pragma once

#include "analysis/slice/SliceParameters.h"
#include "data/model/FocusResult.h"

#include <cstddef>
#include <vector>

namespace beamlab::analysis {

class SlicePlanner {
public:
    [[nodiscard]] std::vector<std::size_t> planFrameSlices(const beamlab::data::FocusResult& focus,
                                                           const SliceParameters& parameters) const;
};

} // namespace beamlab::analysis
