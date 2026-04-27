#pragma once

#include "data/model/BeamEnvelope.h"
#include "data/model/BeamSlice.h"
#include "data/model/TrajectoryDataset.h"
#include "io/common/ExportResult.h"
#include "io/exporters/IExporter.h"

#include <cstddef>
#include <string>
#include <vector>

namespace beamlab::io {

class VisualizationDataExporter final : public IExporter {
public:
    [[nodiscard]] std::string name() const override;

    [[nodiscard]] ExportResult exportTrajectoryPreview(
        const beamlab::data::TrajectoryDataset& dataset,
        const std::string& output_path,
        std::size_t max_trajectories,
        std::size_t max_samples_per_trajectory) const;

    [[nodiscard]] ExportResult exportFocalSlicePoints(
        const beamlab::data::BeamSlice& slice,
        const std::string& output_path,
        std::size_t max_points) const;

    [[nodiscard]] ExportResult exportEnvelopeRings(
        const std::vector<beamlab::data::BeamEnvelope>& envelopes,
        const beamlab::data::AxisFrame& axis_frame,
        const std::string& output_path) const;
};

} // namespace beamlab::io
