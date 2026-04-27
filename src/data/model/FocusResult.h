#pragma once

#include "data/model/FocusCurve.h"

#include <string>

namespace beamlab::data {

struct FocusResult {
    FocusCurve curve{};
    std::size_t focus_index{0};
    double focus_reference_value{0.0};
    double focus_metric_value{0.0};
    double confidence{0.0};
    std::string detection_method{};
    bool valid{false};
};

} // namespace beamlab::data
