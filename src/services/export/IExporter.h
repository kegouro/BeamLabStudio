#pragma once

#include "services/storage/IStorageBackend.h"
#include "services/analysis/JobScheduler.h"  // AnalysisResult

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace beamlab::services::export_ {

/// Progress callback for export operations.
using ExportProgressCallback = std::function<void(float percent, const std::string& stage)>;

/// Result of a single export operation.
struct ExportResult {
    bool success{false};
    std::optional<std::string> error;
    std::filesystem::path outputPath;
    uint64_t bytesWritten{0};
};

/// Abstract interface for a format plugin.
///
/// Each concrete exporter writes analysis results to a specific format
/// (CSV, OBJ, PNG, PDF, Parquet, ...).  Exporters are registered in
/// the ExporterRegistry and can be invoked individually or in batch.
class IExporter {
public:
    virtual ~IExporter() = default;

    /// Human-readable name, e.g. "CSV Exporter".
    [[nodiscard]] virtual std::string name() const = 0;

    /// Short format identifier, e.g. "csv", "obj", "png".
    /// Used as key in ExporterRegistry::exportAll().
    [[nodiscard]] virtual std::string format() const = 0;

    /// File extensions this exporter produces, e.g. {".csv"}.
    [[nodiscard]] virtual std::vector<std::string> fileExtensions() const = 0;

    /// Export analysis results to the specified path.
    ///
    /// \param storage     Backend containing imported samples (for raw data export).
    /// \param result      Analysis results (engine metrics, frame data).
    /// \param outputPath  Directory or file path for the output.
    /// \param onProgress  Optional progress callback.
    virtual ExportResult exportData(
        const storage::IStorageBackend& storage,
        const analysis::AnalysisResult& result,
        const std::filesystem::path& outputPath,
        ExportProgressCallback onProgress = nullptr) = 0;
};

} // namespace beamlab::services::export_
