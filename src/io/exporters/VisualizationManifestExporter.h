#pragma once

#include "io/common/ExportResult.h"
#include "io/exporters/AnalysisReportExporter.h"
#include "io/exporters/IExporter.h"

#include <cstddef>
#include <string>

namespace beamlab::io {

struct VisualizationManifestStats {
    std::size_t trajectory_preview_rows{0};
    std::size_t focal_slice_point_rows{0};
    std::size_t envelope_ring_rows{0};
};

class VisualizationManifestExporter final : public IExporter {
public:
    [[nodiscard]] std::string name() const override;

    [[nodiscard]] ExportResult exportManifest(
        const AnalysisOutputManifest& manifest,
        const AnalysisRunConfiguration& configuration,
        const VisualizationManifestStats& stats,
        const std::string& output_path) const;
};

} // namespace beamlab::io
