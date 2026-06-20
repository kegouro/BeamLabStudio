#include "core/pipeline/AnalysisPipeline.h"

#include "analysis/statistics/BatchStatisticsEngine.h"
#include "core/storage/ISampleStorage.h"
#include "core/storage/SqliteStorage.h"
#include "data/model/AxisFrame.h"
#include "io/importers/Geant4CsvImporter.h"

#include <filesystem>
#include <fstream>
#include <sys/stat.h>

namespace beamlab::core {

AnalysisPipeline::AnalysisPipeline(const AnalysisRunConfig& config)
    : config_(config)
{
}

PipelineResult AnalysisPipeline::run(const std::string& csvPath,
                                      const std::string& outputDir,
                                      ProgressTracker& progress)
{
    PipelineResult result;
    result.outputDir = outputDir;

    // Determine file size for progress
    int64_t fileSize = 0;
    struct stat st;
    if (stat(csvPath.c_str(), &st) == 0) fileSize = st.st_size;

    // Create appropriate storage
    auto estimatedBytes = static_cast<uint64_t>(fileSize > 0 ? fileSize : 0);
    auto storage = ISampleStorage::create(estimatedBytes);

    // Phase 1: Import directly to storage (streaming, O(1) RAM)
    progress.onProgress({0.0, "importing", 0, fileSize});
    progress.onLogLine("Importing: " + csvPath);

    beamlab::io::Geant4CsvImporter importer;
    uint64_t imported = importer.importStreaming(csvPath, *storage, &progress);

    // Build indices AFTER bulk import (5-10x faster than with maintenance during insert)
    storage->finalizeStorage();

    if (progress.cancelled()) {
        result.success = false;
        result.errorMessage = "Cancelled";
        progress.onComplete(false, result.errorMessage);
        return result;
    }

    if (imported == 0) {
        result.success = false;
        result.errorMessage = "No samples imported";
        progress.onComplete(false, result.errorMessage);
        return result;
    }

    progress.onLogLine("Imported " + std::to_string(imported) + " samples, "
                       + std::to_string(storage->trajectoryCount()) + " trajectories");

    // Phase 2: Statistics (batch processing from storage)
    uint64_t sampleCount = storage->totalSampleCount();
    progress.onProgress({0.5, "statistics", 0, static_cast<int64_t>(sampleCount)});
    progress.onLogLine("Computing statistics for " + std::to_string(sampleCount) + " samples");

    // Use a default axis frame (z as longitudinal)
    beamlab::data::AxisFrame axisFrame{};
    axisFrame.origin = {0, 0, 0};
    axisFrame.longitudinal = {0, 0, 1};
    axisFrame.transverse_u = {1, 0, 0};
    axisFrame.transverse_v = {0, 1, 0};

    auto statsEngine = beamlab::analysis::BatchStatisticsEngine{};
    beamlab::analysis::FrameStatisticsParameters statsParams;
    statsParams.reference_mode = beamlab::analysis::ReferenceMode::AxialBins;
    statsParams.axial_binning_mode = beamlab::analysis::AxialBinningMode::Uniform;
    statsParams.axial_bin_count = 501;
    auto stats = statsEngine.compute(*storage, axisFrame, statsParams);
    progress.onLogLine("Statistics: " + std::to_string(stats.size()) + " frames");

    // Phase 3: Export
    progress.onProgress({0.8, "exporting", 0, 0});
    std::filesystem::create_directories(outputDir);
    std::string manifestPath = outputDir + "/analysis_summary.txt";
    std::ofstream out(manifestPath);
    if (out) {
        out << "BeamLabStudio Analysis Summary\n";
        out << "Input: " << csvPath << "\n";
        out << "Trajectories: " << storage->trajectoryCount() << "\n";
        out << "Samples: " << sampleCount << "\n";
        out << "Statistics frames: " << stats.size() << "\n";
    }

    result.success = true;
    result.manifestPath = manifestPath;
    progress.onProgress({1.0, "complete", fileSize, fileSize});
    progress.onComplete(true, "Analysis complete");
    return result;
}

} // namespace beamlab::core
