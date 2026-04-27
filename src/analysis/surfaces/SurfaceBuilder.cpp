#include "analysis/surfaces/SurfaceBuilder.h"

#include "analysis/surfaces/ContourResampler.h"

#include <algorithm>

namespace beamlab::analysis {
namespace {

beamlab::core::Vec3 add(const beamlab::core::Vec3& a, const beamlab::core::Vec3& b)
{
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

beamlab::core::Vec3 scale(const beamlab::core::Vec3& v, const double factor)
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

} // namespace

beamlab::data::LensSurfaceModel SurfaceBuilder::build(
    const std::vector<beamlab::data::BeamEnvelope>& envelopes,
    const beamlab::data::AxisFrame& axis_frame,
    const SurfaceBuildParameters& parameters) const
{
    beamlab::data::LensSurfaceModel surface{};
    surface.name = "effective_lens_surface";
    surface.method_name = "lofted_convex_hull_envelopes";
    surface.points_per_slice = parameters.resample_point_count;

    std::vector<beamlab::data::BeamEnvelope> valid_envelopes{};
    valid_envelopes.reserve(envelopes.size());

    for (const auto& envelope : envelopes) {
        if (envelope.valid && envelope.boundary_points.size() >= 3) {
            valid_envelopes.push_back(envelope);
        }
    }

    if (valid_envelopes.size() < 2 || parameters.resample_point_count < 3) {
        surface.valid = false;
        return surface;
    }

    std::sort(valid_envelopes.begin(), valid_envelopes.end(),
              [](const auto& a, const auto& b) {
                  return a.axial_position_m < b.axial_position_m;
              });

    surface.slice_count = valid_envelopes.size();
    surface.axial_min_m = valid_envelopes.front().axial_position_m;
    surface.axial_max_m = valid_envelopes.back().axial_position_m;
    surface.mesh.name = surface.name;

    const ContourResampler resampler{};

    for (const auto& envelope : valid_envelopes) {
        const auto ring = resampler.resampleClosedContour(envelope.boundary_points,
                                                          parameters.resample_point_count);

        for (const auto& uv : ring) {
            beamlab::geometry::MeshVertex vertex{};
            vertex.position = fromLocalToWorld(uv, envelope.axial_position_m, axis_frame);
            surface.mesh.vertices.push_back(vertex);
        }
    }

    const std::size_t ring_count = valid_envelopes.size();
    const std::size_t points_per_ring = parameters.resample_point_count;

    for (std::size_t ring = 0; ring + 1 < ring_count; ++ring) {
        const std::size_t current = ring * points_per_ring;
        const std::size_t next = (ring + 1) * points_per_ring;

        for (std::size_t i = 0; i < points_per_ring; ++i) {
            const std::size_t i_next = (i + 1) % points_per_ring;

            const std::size_t a = current + i;
            const std::size_t b = current + i_next;
            const std::size_t c = next + i;
            const std::size_t d = next + i_next;

            surface.mesh.faces.push_back({a, c, b});
            surface.mesh.faces.push_back({b, c, d});
        }
    }

    surface.valid = !surface.mesh.empty();
    return surface;
}

} // namespace beamlab::analysis
