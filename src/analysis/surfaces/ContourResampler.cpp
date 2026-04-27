#include "analysis/surfaces/ContourResampler.h"

#include <cmath>
#include <vector>

namespace beamlab::analysis {
namespace {

double distance(const beamlab::core::Vec2& a, const beamlab::core::Vec2& b)
{
    const double dx = a.x - b.x;
    const double dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

beamlab::core::Vec2 interpolate(const beamlab::core::Vec2& a,
                                const beamlab::core::Vec2& b,
                                const double t)
{
    return {
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t
    };
}

} // namespace

std::vector<beamlab::core::Vec2> ContourResampler::resampleClosedContour(
    const std::vector<beamlab::core::Vec2>& points,
    const std::size_t target_count) const
{
    std::vector<beamlab::core::Vec2> result{};

    if (points.empty() || target_count == 0) {
        return result;
    }

    if (points.size() == 1) {
        result.assign(target_count, points.front());
        return result;
    }

    std::vector<double> cumulative{};
    cumulative.reserve(points.size() + 1);
    cumulative.push_back(0.0);

    double perimeter = 0.0;
    for (std::size_t i = 0; i < points.size(); ++i) {
        const auto& a = points[i];
        const auto& b = points[(i + 1) % points.size()];
        perimeter += distance(a, b);
        cumulative.push_back(perimeter);
    }

    if (perimeter <= 0.0) {
        result.assign(target_count, points.front());
        return result;
    }

    result.reserve(target_count);

    std::size_t segment = 0;

    for (std::size_t k = 0; k < target_count; ++k) {
        const double target = perimeter * static_cast<double>(k) /
                              static_cast<double>(target_count);

        while (segment + 1 < cumulative.size() && cumulative[segment + 1] < target) {
            ++segment;
        }

        const std::size_t i0 = segment % points.size();
        const std::size_t i1 = (segment + 1) % points.size();

        const double start = cumulative[segment];
        const double end = cumulative[segment + 1];
        const double denom = end - start;

        const double t = denom > 0.0 ? (target - start) / denom : 0.0;
        result.push_back(interpolate(points[i0], points[i1], t));
    }

    return result;
}

} // namespace beamlab::analysis
