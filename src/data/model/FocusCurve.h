#pragma once

#include <string>
#include <vector>

namespace beamlab::data {

struct FocusCurvePoint {
    double reference_value{0.0};
    double reference_min_value{0.0};
    double reference_max_value{0.0};
    double metric_value{0.0};
    std::size_t point_count{0};
};

struct FocusCurve {
    std::string metric_name{};
    std::string reference_name{"axial_position_m"};
    std::vector<FocusCurvePoint> points{};
};

} // namespace beamlab::data
