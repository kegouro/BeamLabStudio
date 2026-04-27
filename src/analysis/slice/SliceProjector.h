#pragma once

#include "core/math/Vec2.h"
#include "core/math/Vec3.h"
#include "data/model/AxisFrame.h"

namespace beamlab::analysis {

class SliceProjector {
public:
    [[nodiscard]] beamlab::core::Vec2 projectToTransversePlane(const beamlab::core::Vec3& position,
                                                               const beamlab::data::AxisFrame& frame) const;

    [[nodiscard]] double projectToLongitudinalAxis(const beamlab::core::Vec3& position,
                                                   const beamlab::data::AxisFrame& frame) const;
};

} // namespace beamlab::analysis
