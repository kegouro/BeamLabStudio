#include "analysis/surfaces/LensDiskBuilder.h"

#include "analysis/surfaces/ContourResampler.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace beamlab::analysis {
namespace {

beamlab::core::Vec2 centroid2D(const std::vector<beamlab::core::Vec2>& points)
{
    beamlab::core::Vec2 c{};

    if (points.empty()) {
        return c;
    }

    for (const auto& p : points) {
        c.x += p.x;
        c.y += p.y;
    }

    c.x /= static_cast<double>(points.size());
    c.y /= static_cast<double>(points.size());
    return c;
}

beamlab::core::Vec2 interpolate2D(const beamlab::core::Vec2& a,
                                  const beamlab::core::Vec2& b,
                                  const double t)
{
    return {
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t
    };
}

beamlab::core::Vec3 add(const beamlab::core::Vec3& a,
                        const beamlab::core::Vec3& b)
{
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

beamlab::core::Vec3 scale(const beamlab::core::Vec3& v,
                          const double factor)
{
    return {v.x * factor, v.y * factor, v.z * factor};
}

beamlab::core::Vec3 fromLocalToWorld(const beamlab::core::Vec2& uv,
                                     const double axial,
                                     const beamlab::data::AxisFrame& frame)
{
    auto position = frame.origin;
    position = add(position, scale(frame.transverse_u, uv.x));
    position = add(position, scale(frame.transverse_v, uv.y));
    position = add(position, scale(frame.longitudinal, axial));
    return position;
}

double thicknessAtRadiusFraction(const double s,
                                 const LensDiskBuildParameters& parameters)
{
    const double clamped_s = std::max(0.0, std::min(1.0, s));
    const double blend = 1.0 - clamped_s * clamped_s;

    return parameters.edge_thickness_m +
           (parameters.center_thickness_m - parameters.edge_thickness_m) * blend;
}

} // namespace

beamlab::data::LensSurfaceModel LensDiskBuilder::build(
    const beamlab::data::BeamEnvelope& focal_envelope,
    const beamlab::data::AxisFrame& axis_frame,
    const LensDiskBuildParameters& parameters) const
{
    beamlab::data::LensSurfaceModel lens{};
    lens.name = "effective_lens_disk";
    lens.method_name = "biconvex_disk_from_focal_envelope";

    if (!focal_envelope.valid ||
        focal_envelope.boundary_points.size() < 3 ||
        parameters.boundary_point_count < 8 ||
        parameters.radial_layers < 1 ||
        parameters.center_thickness_m <= 0.0 ||
        parameters.edge_thickness_m < 0.0) {
        lens.valid = false;
        return lens;
    }

    const ContourResampler resampler{};
    const auto boundary = resampler.resampleClosedContour(
        focal_envelope.boundary_points,
        parameters.boundary_point_count
    );

    if (boundary.size() < 3) {
        lens.valid = false;
        return lens;
    }

    const auto center_uv = centroid2D(boundary);
    const std::size_t n = boundary.size();
    const std::size_t layers = parameters.radial_layers;

    lens.slice_count = 1;
    lens.points_per_slice = n;
    lens.axial_min_m = focal_envelope.axial_position_m - 0.5 * parameters.center_thickness_m;
    lens.axial_max_m = focal_envelope.axial_position_m + 0.5 * parameters.center_thickness_m;
    lens.mesh.name = lens.name;

    const std::size_t top_center_index = 0;
    const std::size_t bottom_center_index = 1;

    {
        beamlab::geometry::MeshVertex top_center{};
        top_center.position = fromLocalToWorld(
            center_uv,
            focal_envelope.axial_position_m + 0.5 * parameters.center_thickness_m,
            axis_frame
        );

        beamlab::geometry::MeshVertex bottom_center{};
        bottom_center.position = fromLocalToWorld(
            center_uv,
            focal_envelope.axial_position_m - 0.5 * parameters.center_thickness_m,
            axis_frame
        );

        lens.mesh.vertices.push_back(top_center);
        lens.mesh.vertices.push_back(bottom_center);
    }

    const std::size_t top_rings_start = lens.mesh.vertices.size();

    for (std::size_t layer = 1; layer <= layers; ++layer) {
        const double s = static_cast<double>(layer) / static_cast<double>(layers);
        const double local_thickness = thicknessAtRadiusFraction(s, parameters);

        for (const auto& boundary_point : boundary) {
            const auto uv = interpolate2D(center_uv, boundary_point, s);

            beamlab::geometry::MeshVertex vertex{};
            vertex.position = fromLocalToWorld(
                uv,
                focal_envelope.axial_position_m + 0.5 * local_thickness,
                axis_frame
            );

            lens.mesh.vertices.push_back(vertex);
        }
    }

    const std::size_t bottom_rings_start = lens.mesh.vertices.size();

    for (std::size_t layer = 1; layer <= layers; ++layer) {
        const double s = static_cast<double>(layer) / static_cast<double>(layers);
        const double local_thickness = thicknessAtRadiusFraction(s, parameters);

        for (const auto& boundary_point : boundary) {
            const auto uv = interpolate2D(center_uv, boundary_point, s);

            beamlab::geometry::MeshVertex vertex{};
            vertex.position = fromLocalToWorld(
                uv,
                focal_envelope.axial_position_m - 0.5 * local_thickness,
                axis_frame
            );

            lens.mesh.vertices.push_back(vertex);
        }
    }

    const auto topIndex = [top_rings_start, n](std::size_t layer, std::size_t i) {
        return top_rings_start + (layer - 1) * n + (i % n);
    };

    const auto bottomIndex = [bottom_rings_start, n](std::size_t layer, std::size_t i) {
        return bottom_rings_start + (layer - 1) * n + (i % n);
    };

    // Tapa superior: centro hacia primer anillo.
    for (std::size_t i = 0; i < n; ++i) {
        const std::size_t j = (i + 1) % n;
        lens.mesh.faces.push_back({top_center_index, topIndex(1, i), topIndex(1, j)});
    }

    // Tapa superior: anillos intermedios.
    for (std::size_t layer = 1; layer < layers; ++layer) {
        for (std::size_t i = 0; i < n; ++i) {
            const std::size_t j = (i + 1) % n;

            const std::size_t a = topIndex(layer, i);
            const std::size_t b = topIndex(layer, j);
            const std::size_t c = topIndex(layer + 1, i);
            const std::size_t d = topIndex(layer + 1, j);

            lens.mesh.faces.push_back({a, c, b});
            lens.mesh.faces.push_back({b, c, d});
        }
    }

    // Tapa inferior, con orientación invertida.
    for (std::size_t i = 0; i < n; ++i) {
        const std::size_t j = (i + 1) % n;
        lens.mesh.faces.push_back({bottom_center_index, bottomIndex(1, j), bottomIndex(1, i)});
    }

    for (std::size_t layer = 1; layer < layers; ++layer) {
        for (std::size_t i = 0; i < n; ++i) {
            const std::size_t j = (i + 1) % n;

            const std::size_t a = bottomIndex(layer, i);
            const std::size_t b = bottomIndex(layer, j);
            const std::size_t c = bottomIndex(layer + 1, i);
            const std::size_t d = bottomIndex(layer + 1, j);

            lens.mesh.faces.push_back({a, b, c});
            lens.mesh.faces.push_back({b, d, c});
        }
    }

    // Pared lateral exterior.
    for (std::size_t i = 0; i < n; ++i) {
        const std::size_t j = (i + 1) % n;

        const std::size_t top_i = topIndex(layers, i);
        const std::size_t top_j = topIndex(layers, j);
        const std::size_t bottom_i = bottomIndex(layers, i);
        const std::size_t bottom_j = bottomIndex(layers, j);

        lens.mesh.faces.push_back({top_i, bottom_i, top_j});
        lens.mesh.faces.push_back({top_j, bottom_i, bottom_j});
    }

    lens.valid = !lens.mesh.empty();
    return lens;
}

} // namespace beamlab::analysis
