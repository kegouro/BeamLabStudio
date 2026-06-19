#include "services/export/CsvExporter.h"

#include <filesystem>
#include <fstream>
#include <numeric>

namespace fs = std::filesystem;

namespace beamlab::services::export_ {

ExportResult CsvExporter::exportData(
    const storage::IStorageBackend& storage,
    const analysis::AnalysisResult& result,
    const fs::path& outputPath,
    ExportProgressCallback onProgress)
{
    if (onProgress) onProgress(0.0f, "Preparing CSV export");

    auto csvPath = outputPath;
    if (fs::is_directory(outputPath)) {
        csvPath = outputPath / "frame_statistics.csv";
    }

    std::ofstream out(csvPath);
    if (!out.is_open()) {
        return ExportResult{false, "Cannot write " + csvPath.string(), csvPath, 0};
    }

    // Header.
    out << "frame_index,reference_value,reference_min,reference_max,"
           "point_count,centroid_u,centroid_v,sigma_u,sigma_v,r_rms,valid\n";

    uint64_t bytesWritten = 0;
    int frameIndex = 0;

    // Iterate through all engine results and extract frame data.
    for (const auto& [engineName, engineRes] : result.engineResults) {
        if (!engineRes.metrics.contains("frames_data")) continue;

        for (const auto& frame : engineRes.metrics["frames_data"]) {
            out << frameIndex << ","
                << frame.value("reference_value", 0.0) << ","
                << frame.value("reference_min_value", 0.0) << ","
                << frame.value("reference_max_value", 0.0) << ","
                << frame.value("point_count", 0) << ","
                << frame.value("centroid_u", 0.0) << ","
                << frame.value("centroid_v", 0.0) << ","
                << frame.value("sigma_u", 0.0) << ","
                << frame.value("sigma_v", 0.0) << ","
                << frame.value("r_rms", 0.0) << ","
                << (frame.value("valid", true) ? "true" : "false")
                << "\n";
            ++frameIndex;
        }
    }

    // If there are no frames_data in engine results, try top-level metrics.
    if (frameIndex == 0 && result.engineResults.empty()) {
        // Write metadata as a fallback.
        out << "# No frame data available. Analysis result:\n";
        out << "# success=" << (result.success ? "true" : "false") << "\n";
        if (result.error) out << "# error=" << *result.error << "\n";
        out << "# duration_ms=" << result.duration.count() << "\n";
    }

    bytesWritten = static_cast<uint64_t>(out.tellp());
    out.close();

    if (onProgress) onProgress(1.0f, "CSV export complete");

    return ExportResult{true, std::nullopt, csvPath, bytesWritten};
}

} // namespace beamlab::services::export_
