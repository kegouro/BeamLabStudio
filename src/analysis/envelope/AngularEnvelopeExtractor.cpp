#include "analysis/envelope/AngularEnvelopeExtractor.h"

#include <cmath>
#include <limits>
#include <numbers>
#include <vector>

namespace beamlab::analysis {
namespace {

double distance(const beamlab::core::Vec2& a, const beamlab::core::Vec2& b)
{
    const double dx = a.x - b.x;
    const double dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

double polygonArea(const std::vector<beamlab::core::Vec2>& points)
{
    if (points.size() < 3) {
        return 0.0;
    }

    double area = 0.0;

    for (std::size_t i = 0; i < points.size(); ++i) {
        const auto& a = points[i];
        const auto& b = points[(i + 1) % points.size()];
        area += a.x * b.y - b.x * a.y;
    }

    return 0.5 * std::abs(area);
}

double polygonPerimeter(const std::vector<beamlab::core::Vec2>& points)
{
    if (points.size() < 2) {
        return 0.0;
    }

    double perimeter = 0.0;

    for (std::size_t i = 0; i < points.size(); ++i) {
        perimeter += distance(points[i], points[(i + 1) % points.size()]);
    }

    return perimeter;
}

} // namespace

std::string AngularEnvelopeExtractor::name() const
{
    return "angular_max_radius";
}

beamlab::data::BeamEnvelope AngularEnvelopeExtractor::extract(const beamlab::data::BeamSlice& slice,
                                                              const EnvelopeParameters& parameters) const
{
    beamlab::data::BeamEnvelope envelope{};
    envelope.slice_index = slice.slice_index;
    envelope.reference_time_s = slice.reference_time_s;
    envelope.axial_position_m = slice.axial_position_m;
    envelope.method_name = name();
    envelope.input_point_count = slice.points.size();

    if (slice.points.size() < parameters.minimum_points || parameters.angular_bins < 8) {
        envelope.valid = false;
        return envelope;
    }

    std::vector<double> best_radius(parameters.angular_bins, -std::numeric_limits<double>::infinity());
    std::vector<beamlab::core::Vec2> best_point(parameters.angular_bins);

    for (const auto& point : slice.points) {
        const double x = point.projected_uv_m.x;
        const double y = point.projected_uv_m.y;
        const double r = std::sqrt(x * x + y * y);

        double theta = std::atan2(y, x);
        if (theta < 0.0) {
            theta += 2.0 * std::numbers::pi;
        }

        const auto bin = static_cast<std::size_t>(
            std::floor(theta / (2.0 * std::numbers::pi) * static_cast<double>(parameters.angular_bins))
        );

        const std::size_t safe_bin = bin >= parameters.angular_bins ? parameters.angular_bins - 1 : bin;

        if (r > best_radius[safe_bin]) {
            best_radius[safe_bin] = r;
            best_point[safe_bin] = point.projected_uv_m;
        }
    }

    for (std::size_t i = 0; i < parameters.angular_bins; ++i) {
        if (best_radius[i] > 0.0) {
            envelope.boundary_points.push_back(best_point[i]);
        }
    }

    envelope.area_m2 = polygonArea(envelope.boundary_points);
    envelope.perimeter_m = polygonPerimeter(envelope.boundary_points);
    envelope.valid = envelope.boundary_points.size() >= parameters.minimum_points;

    return envelope;
}

} // namespace beamlab::analysis
