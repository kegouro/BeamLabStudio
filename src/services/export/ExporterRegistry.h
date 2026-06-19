#pragma once

#include "services/export/IExporter.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace beamlab::services::export_ {

/// Registry of IExporter plugins with batch-export capability.
///
/// ## Usage
///
/// ```cpp
/// ExporterRegistry reg;
/// reg.registerExporter(std::make_unique<CsvExporter>());
/// reg.registerExporter(std::make_unique<ObjExporter>());
///
/// // Export all formats:
/// auto result = reg.exportAll(storage, analysisResult, "/output/dir");
///
/// // Export specific formats only:
/// reg.exportAll(storage, analysisResult, "/output/dir", {"csv", "obj"});
///
/// // Export single format:
/// reg.exportSingle("csv", storage, analysisResult, "/output/dir/data.csv");
/// ```
class ExporterRegistry {
public:
    /// Aggregate result across multiple format exports.
    struct MultiExportResult {
        bool overallSuccess{false};
        std::vector<std::pair<std::string, ExportResult>> results;
    };

    /// Register an exporter plugin.  Takes ownership.
    void registerExporter(std::unique_ptr<IExporter> exporter);

    /// Export to all specified formats (or all registered if empty).
    ///
    /// If outputPath is a directory, each format writes into a
    /// subdirectory: outputDir/csv/, outputDir/obj/, etc.
    /// If a format fails, overallSuccess becomes false but other
    /// formats continue.
    MultiExportResult exportAll(
        const storage::IStorageBackend& storage,
        const analysis::AnalysisResult& result,
        const std::filesystem::path& outputDir,
        const std::vector<std::string>& formats = {});

    /// Export to a single format.
    ///
    /// \throws std::out_of_range if the format is not registered.
    ExportResult exportSingle(
        const std::string& format,
        const storage::IStorageBackend& storage,
        const analysis::AnalysisResult& result,
        const std::filesystem::path& outputPath);

    /// List all available format identifiers (e.g. {"csv", "obj"}).
    [[nodiscard]] std::vector<std::string> availableFormats() const;

    /// Number of registered exporters.
    [[nodiscard]] std::size_t count() const { return exporters_.size(); }

private:
    std::vector<std::unique_ptr<IExporter>> exporters_;
    std::unordered_map<std::string, IExporter*> byFormat_;
};

} // namespace beamlab::services::export_
