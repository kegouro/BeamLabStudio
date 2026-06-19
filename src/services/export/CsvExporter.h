#pragma once

#include "services/export/IExporter.h"

namespace beamlab::services::export_ {

/// Exports analysis frame statistics to CSV.
///
/// Produces a file with columns:
///   frame_index, reference_value, reference_min, reference_max,
///   point_count, centroid_u, centroid_v, sigma_u, sigma_v, r_rms, valid
class CsvExporter final : public IExporter {
public:
    std::string name() const override { return "CSV Exporter"; }
    std::string format() const override { return "csv"; }
    std::vector<std::string> fileExtensions() const override { return {".csv"}; }

    ExportResult exportData(
        const storage::IStorageBackend& storage,
        const analysis::AnalysisResult& result,
        const std::filesystem::path& outputPath,
        ExportProgressCallback onProgress) override;
};

} // namespace beamlab::services::export_
