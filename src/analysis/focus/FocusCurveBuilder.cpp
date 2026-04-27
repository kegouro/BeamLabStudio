#include "analysis/focus/FocusCurveBuilder.h"

namespace beamlab::analysis {
namespace {

const char* metricName(const FocusMetricType metric)
{
    switch (metric) {
    case FocusMetricType::TransverseRmsRadius:
        return "transverse_r_rms";
    case FocusMetricType::SigmaU:
        return "sigma_u";
    case FocusMetricType::SigmaV:
        return "sigma_v";
    }
    return "unknown";
}

double metricValue(const FrameStatistics& stats, const FocusMetricType metric)
{
    switch (metric) {
    case FocusMetricType::TransverseRmsRadius:
        return stats.r_rms;
    case FocusMetricType::SigmaU:
        return stats.sigma_u;
    case FocusMetricType::SigmaV:
        return stats.sigma_v;
    }
    return stats.r_rms;
}

} // namespace

beamlab::data::FocusCurve FocusCurveBuilder::build(const std::vector<FrameStatistics>& frame_stats,
                                                   const FocusParameters& parameters) const
{
    beamlab::data::FocusCurve curve{};
    curve.metric_name = metricName(parameters.metric);
    curve.reference_name = "axial_position_m";

    curve.points.reserve(frame_stats.size());

    for (const auto& stats : frame_stats) {
        beamlab::data::FocusCurvePoint point{};
        point.reference_value = stats.reference_value;
        point.reference_min_value = stats.reference_min_value;
        point.reference_max_value = stats.reference_max_value;
        point.metric_value = metricValue(stats, parameters.metric);
        point.point_count = stats.point_count;
        curve.points.push_back(point);
    }

    return curve;
}

} // namespace beamlab::analysis
