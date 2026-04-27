#include "analysis/slice/SliceProjector.h"

namespace beamlab::analysis {
namespace {

beamlab::core::Vec3 subtract(const beamlab::core::Vec3& a, const beamlab::core::Vec3& b)
{
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

double dot(const beamlab::core::Vec3& a, const beamlab::core::Vec3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

} // namespace

beamlab::core::Vec2 SliceProjector::projectToTransversePlane(const beamlab::core::Vec3& position,
                                                             const beamlab::data::AxisFrame& frame) const
{
    const auto relative = subtract(position, frame.origin);

    return {
        dot(relative, frame.transverse_u),
        dot(relative, frame.transverse_v)
    };
}

double SliceProjector::projectToLongitudinalAxis(const beamlab::core::Vec3& position,
                                                 const beamlab::data::AxisFrame& frame) const
{
    const auto relative = subtract(position, frame.origin);
    return dot(relative, frame.longitudinal);
}

} // namespace beamlab::analysis
