#pragma once

#include "analysis/surfaces/SurfaceBuildParameters.h"
#include "data/model/AxisFrame.h"
#include "data/model/BeamEnvelope.h"
#include "data/model/LensSurfaceModel.h"

#include <vector>

namespace beamlab::analysis {

class SurfaceBuilder {
public:
    [[nodiscard]] beamlab::data::LensSurfaceModel build(
        const std::vector<beamlab::data::BeamEnvelope>& envelopes,
        const beamlab::data::AxisFrame& axis_frame,
        const SurfaceBuildParameters& parameters) const;
};

} // namespace beamlab::analysis
