#include "analysis/slice/SliceProjector.h"

namespace beamlab::analysis {
namespace {
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
