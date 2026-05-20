#include "core/pipeline/AnalysisPipeline.h"

#include "analysis/statistics/BatchStatisticsEngine.h"
#include "core/storage/ISampleStorage.h"
#include "core/storage/InMemoryStorage.h"
#include "data/model/AxisFrame.h"
#include "io/importers/Geant4CsvImporter.h"
#include "io/importers/IImporter.h"

#include <filesystem>
#include <fstream>

namespace beamlab::core {

static uint64_t totalSamples(const beamlab::data::TrajectoryDataset& ds)
{
    uint64_t n = 0;
    for (const auto& t : ds.trajectories) n += t.samples.size();
    return n;
}

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

    // Phase 1: Import
    progress.onProgress({0.0, "importing", 0, 0});
    progress.onLogLine("Importing: " + csvPath);

    beamlab::io::Geant4CsvImporter importer;
    auto inspectReport = importer.inspect(csvPath);
    if (!inspectReport.readable) {
        result.success = false;
        result.errorMessage = "Cannot read file: " + csvPath;
        progress.onComplete(false, result.errorMessage);
        return result;
    }

    auto schema = importer.buildSchema(csvPath, inspectReport);
    beamlab::io::ImportContext ctx;
    ctx.preferred_display_name =
        std::filesystem::path(csvPath).stem().string();

    auto importResult = importer.import(csvPath, schema, ctx);
    if (!importResult.success || !importResult.dataset.has_value()) {
        result.success = false;
        result.errorMessage = "Import failed";
        progress.onComplete(false, result.errorMessage);
        return result;
    }

    auto& dataset = *importResult.dataset;
    progress.onLogLine("Imported " + std::to_string(dataset.trajectories.size())
                       + " trajectories");

    if (progress.cancelled()) {
        result.success = false;
        result.errorMessage = "Cancelled";
        progress.onComplete(false, result.errorMessage);
        return result;
    }

    // Phase 2: Store in ISampleStorage
    progress.onProgress({0.3, "storing", 0, 0});
    auto storage = std::make_unique<InMemoryStorage>();
    for (const auto& traj : dataset.trajectories) {
        std::string tid = std::to_string(traj.particle.event_id)
                        + "_" + std::to_string(traj.particle.track_id);
        storage->beginTrajectory(tid);
        for (const auto& s : traj.samples) storage->addSample(s);
        storage->endTrajectory();
    }
    storage->flush();

    if (progress.cancelled()) {
        result.success = false;
        result.errorMessage = "Cancelled";
        progress.onComplete(false, result.errorMessage);
        return result;
    }

    // Phase 3: Statistics
    uint64_t sampleCount = storage->totalSampleCount();
    progress.onProgress({0.5, "statistics", 0, static_cast<int64_t>(sampleCount)});
    progress.onLogLine("Computing statistics for " + std::to_string(sampleCount)
                       + " samples");

    auto statsEngine = beamlab::analysis::BatchStatisticsEngine{};
    auto stats = statsEngine.compute(*storage, dataset.axis_frame);
    progress.onLogLine("Statistics: " + std::to_string(stats.size()) + " frames");

    // Phase 4: Export manifest
    progress.onProgress({0.8, "exporting", 0, 0});
    std::filesystem::create_directories(outputDir);
    std::string manifestPath = outputDir + "/analysis_summary.txt";
    std::ofstream out(manifestPath);
    if (out) {
        out << "BeamLabStudio Analysis Summary\n";
        out << "Input: " << csvPath << "\n";
        out << "Trajectories: " << dataset.trajectories.size() << "\n";
        out << "Samples: " << sampleCount << "\n";
        out << "Statistics frames: " << stats.size() << "\n";
    }

    result.success = true;
    result.manifestPath = manifestPath;
    progress.onProgress({1.0, "complete", 0, 0});
    progress.onComplete(true, "Analysis complete");
    return result;
}

} // namespace beamlab::core
