#pragma once

#include "analysis/focus/FocusMetricType.h"

namespace beamlab::analysis {

struct FocusParameters {
    FocusMetricType metric{FocusMetricType::TransverseRmsRadius};
    bool smooth_curve{false};
};

} // namespace beamlab::analysis
