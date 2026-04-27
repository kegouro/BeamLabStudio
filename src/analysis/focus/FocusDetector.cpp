#include "analysis/focus/FocusDetector.h"

#include <limits>

namespace beamlab::analysis {

beamlab::data::FocusResult FocusDetector::detect(const beamlab::data::FocusCurve& curve,
                                                 const FocusParameters&) const
{
    beamlab::data::FocusResult result{};
    result.curve = curve;
    result.detection_method = "minimum_metric";

    if (curve.points.empty()) {
        result.valid = false;
        result.confidence = 0.0;
        return result;
    }

    std::size_t best_index = 0;
    double best_value = std::numeric_limits<double>::infinity();

    for (std::size_t i = 0; i < curve.points.size(); ++i) {
        if (curve.points[i].metric_value < best_value) {
            best_value = curve.points[i].metric_value;
            best_index = i;
        }
    }

    result.focus_index = best_index;
    result.focus_reference_value = curve.points[best_index].reference_value;
    result.focus_metric_value = curve.points[best_index].metric_value;
    result.valid = true;

    if (curve.points.size() >= 3 && best_index > 0 && best_index + 1 < curve.points.size()) {
        const double left = curve.points[best_index - 1].metric_value;
        const double center = curve.points[best_index].metric_value;
        const double right = curve.points[best_index + 1].metric_value;
        const double contrast = ((left - center) + (right - center)) * 0.5;
        result.confidence = contrast > 0.0 ? contrast : 0.0;
    } else {
        result.confidence = 0.1;
    }

    return result;
}

} // namespace beamlab::analysis
