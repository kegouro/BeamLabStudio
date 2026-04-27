#pragma once

#include "analysis/focus/FocusParameters.h"
#include "analysis/statistics/FrameStatistics.h"
#include "data/model/FocusCurve.h"

#include <vector>

namespace beamlab::analysis {

class FocusCurveBuilder {
public:
    [[nodiscard]] beamlab::data::FocusCurve build(const std::vector<FrameStatistics>& frame_stats,
                                                  const FocusParameters& parameters) const;
};

} // namespace beamlab::analysis
