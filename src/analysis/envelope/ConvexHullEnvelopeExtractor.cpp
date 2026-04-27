#include "analysis/envelope/ConvexHullEnvelopeExtractor.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace beamlab::analysis {
namespace {

bool lessXY(const beamlab::core::Vec2& a, const beamlab::core::Vec2& b)
{
    if (a.x == b.x) {
        return a.y < b.y;
    }

    return a.x < b.x;
}

double cross(const beamlab::core::Vec2& origin,
             const beamlab::core::Vec2& a,
             const beamlab::core::Vec2& b)
{
    const double ax = a.x - origin.x;
    const double ay = a.y - origin.y;
    const double bx = b.x - origin.x;
    const double by = b.y - origin.y;

    return ax * by - ay * bx;
}

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

std::vector<beamlab::core::Vec2> convexHull(std::vector<beamlab::core::Vec2> points)
{
    if (points.size() <= 1) {
        return points;
    }

    std::sort(points.begin(), points.end(), lessXY);

    points.erase(std::unique(points.begin(), points.end(),
                             [](const auto& a, const auto& b) {
                                 return a.x == b.x && a.y == b.y;
                             }),
                 points.end());

    if (points.size() <= 2) {
        return points;
    }

    std::vector<beamlab::core::Vec2> lower{};
    for (const auto& point : points) {
        while (lower.size() >= 2 &&
               cross(lower[lower.size() - 2], lower[lower.size() - 1], point) <= 0.0) {
            lower.pop_back();
        }
        lower.push_back(point);
    }

    std::vector<beamlab::core::Vec2> upper{};
    for (auto it = points.rbegin(); it != points.rend(); ++it) {
        while (upper.size() >= 2 &&
               cross(upper[upper.size() - 2], upper[upper.size() - 1], *it) <= 0.0) {
            upper.pop_back();
        }
        upper.push_back(*it);
    }

    lower.pop_back();
    upper.pop_back();

    lower.insert(lower.end(), upper.begin(), upper.end());
    return lower;
}

} // namespace

std::string ConvexHullEnvelopeExtractor::name() const
{
    return "convex_hull";
}

beamlab::data::BeamEnvelope ConvexHullEnvelopeExtractor::extract(const beamlab::data::BeamSlice& slice,
                                                                 const EnvelopeParameters& parameters) const
{
    beamlab::data::BeamEnvelope envelope{};
    envelope.slice_index = slice.slice_index;
    envelope.reference_time_s = slice.reference_time_s;
    envelope.axial_position_m = slice.axial_position_m;
    envelope.method_name = name();
    envelope.input_point_count = slice.points.size();

    if (slice.points.size() < parameters.minimum_points) {
        envelope.valid = false;
        return envelope;
    }

    std::vector<beamlab::core::Vec2> projected{};
    projected.reserve(slice.points.size());

    for (const auto& point : slice.points) {
        projected.push_back(point.projected_uv_m);
    }

    envelope.boundary_points = convexHull(std::move(projected));
    envelope.area_m2 = polygonArea(envelope.boundary_points);
    envelope.perimeter_m = polygonPerimeter(envelope.boundary_points);
    envelope.valid = envelope.boundary_points.size() >= parameters.minimum_points;

    return envelope;
}

} // namespace beamlab::analysis
