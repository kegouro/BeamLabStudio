#pragma once

#include "analysis/surfaces/LensDiskBuildParameters.h"
#include "analysis/surfaces/SurfaceBuildParameters.h"
#include "data/model/AxisFrame.h"
#include "data/model/BeamEnvelope.h"
#include "io/common/ExportResult.h"
#include "io/exporters/IExporter.h"

#include <string>
#include <vector>

namespace beamlab::io {

class ParametricSurfaceExporter final : public IExporter {
public:
    [[nodiscard]] std::string name() const override;

    [[nodiscard]] ExportResult exportCausticParametricDescription(
        const std::vector<beamlab::data::BeamEnvelope>& envelopes,
        const beamlab::data::AxisFrame& axis_frame,
        const beamlab::analysis::SurfaceBuildParameters& parameters,
        const std::string& txt_path,
        const std::string& csv_path) const;

    [[nodiscard]] ExportResult exportLensDiskParametricDescription(
        const beamlab::data::BeamEnvelope& focal_envelope,
        const beamlab::data::AxisFrame& axis_frame,
        const beamlab::analysis::LensDiskBuildParameters& parameters,
        const std::string& txt_path,
        const std::string& csv_path) const;
};

} // namespace beamlab::io
