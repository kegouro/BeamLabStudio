#pragma once

#include "analysis/quality/AnalysisQualityReport.h"
#include "data/model/BeamEnvelope.h"
#include "data/model/FocusResult.h"
#include "data/model/LensSurfaceModel.h"
#include "data/model/TrajectoryDataset.h"

#include <vector>

namespace beamlab::analysis {

class AnalysisQualityChecker {
public:
    [[nodiscard]] AnalysisQualityReport check(
        const beamlab::data::TrajectoryDataset& dataset,
        const beamlab::data::FocusResult& focus,
        const std::vector<beamlab::data::BeamEnvelope>& envelopes,
        const beamlab::data::LensSurfaceModel& caustic_surface,
        const beamlab::data::LensSurfaceModel& lens_disk) const;
};

} // namespace beamlab::analysis
