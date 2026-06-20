#include "services/export/ExporterRegistry.h"

#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace beamlab::services::export_ {

void ExporterRegistry::registerExporter(std::unique_ptr<IExporter> exporter)
{
    auto fmt = exporter->format();
    byFormat_[fmt] = exporter.get();
    exporters_.push_back(std::move(exporter));
}

ExporterRegistry::MultiExportResult ExporterRegistry::exportAll(
    const storage::IStorageBackend& storage,
    const analysis::AnalysisResult& inputResult,
    const fs::path& outputDir,
    const std::vector<std::string>& formats)
{
    MultiExportResult multi;
    multi.overallSuccess = true;

    // Determine which formats to export.
    std::vector<std::string> targetFormats = formats;
    if (targetFormats.empty()) {
        targetFormats = availableFormats();
    }

    // Create output directory.
    fs::create_directories(outputDir);

    for (const auto& fmt : targetFormats) {
        auto it = byFormat_.find(fmt);
        if (it == byFormat_.end()) {
            multi.results.emplace_back(fmt, ExportResult{
                false, "Format not registered: " + fmt, outputDir, 0});
            multi.overallSuccess = false;
            continue;
        }

        // Create format-specific subdirectory.
        auto formatDir = outputDir / fmt;
        fs::create_directories(formatDir);

        try {
            auto exportResult = it->second->exportData(
                storage, inputResult, formatDir, nullptr);
            if (!exportResult.success) {
                multi.overallSuccess = false;
            }
            multi.results.emplace_back(fmt, std::move(exportResult));
        } catch (const std::exception& e) {
            multi.overallSuccess = false;
            multi.results.emplace_back(fmt, ExportResult{
                false, e.what(), formatDir, 0});
        }
    }

    return multi;
}

ExportResult ExporterRegistry::exportSingle(
    const std::string& format,
    const storage::IStorageBackend& storage,
    const analysis::AnalysisResult& result,
    const fs::path& outputPath)
{
    auto it = byFormat_.find(format);
    if (it == byFormat_.end()) {
        throw std::out_of_range("Exporter not registered for format: " + format);
    }

    fs::create_directories(outputPath.parent_path());

    return it->second->exportData(storage, result, outputPath, nullptr);
}

std::vector<std::string> ExporterRegistry::availableFormats() const
{
    std::vector<std::string> formats;
    formats.reserve(byFormat_.size());
    for (const auto& [fmt, _] : byFormat_) {
        formats.push_back(fmt);
    }
    return formats;
}

} // namespace beamlab::services::export_
