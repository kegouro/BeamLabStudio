#pragma once

#include "analysis/surfaces/LensDiskBuildParameters.h"
#include "data/model/AxisFrame.h"
#include "data/model/BeamEnvelope.h"
#include "data/model/LensSurfaceModel.h"

namespace beamlab::analysis {

class LensDiskBuilder {
public:
    [[nodiscard]] beamlab::data::LensSurfaceModel build(
        const beamlab::data::BeamEnvelope& focal_envelope,
        const beamlab::data::AxisFrame& axis_frame,
        const LensDiskBuildParameters& parameters) const;
};

} // namespace beamlab::analysis
