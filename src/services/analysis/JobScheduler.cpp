#include "services/analysis/JobScheduler.h"

#include <iostream>
#include <numeric>

namespace beamlab::services::analysis {

// ── Constructor ───────────────────────────────────────────────────────

JobScheduler::JobScheduler(std::size_t threadCount)
    : pool_(threadCount > 0 ? threadCount
                            : std::thread::hardware_concurrency())
{
}

// ── Level builder ─────────────────────────────────────────────────────

std::vector<std::vector<IAnalysisEngine*>> JobScheduler::buildLevels(
    const std::vector<std::unique_ptr<IAnalysisEngine>>& engines) const
{
    // Level 0: engines that work on raw data.
    // Level 1: engines that need binned data (depend on Level 0).
    std::vector<IAnalysisEngine*> level0, level1;

    for (const auto& eng : engines) {
        if (eng->requiresBinnedData()) {
            level1.push_back(eng.get());
        } else {
            level0.push_back(eng.get());
        }
    }

    std::vector<std::vector<IAnalysisEngine*>> levels;
    if (!level0.empty()) levels.push_back(std::move(level0));
    if (!level1.empty()) levels.push_back(std::move(level1));
    return levels;
}

// ── Level executor ────────────────────────────────────────────────────

void JobScheduler::executeLevel(
    const std::vector<IAnalysisEngine*>& level,
    const beamlab::services::storage::IStorageBackend& storage,
    const nlohmann::json& config,
    AnalysisResult& result,
    std::mutex& resultMutex,
    JobProgress& progress,
    JobProgressCallback onProgress,
    std::size_t* samplesProcessed,
    std::size_t totalSamples)
{
    std::atomic<std::size_t> completedInLevel{0};
    std::size_t levelSize = level.size();

    for (auto* engine : level) {
        pool_.enqueue([this, engine, &storage, &config,
                       &result, &resultMutex,
                       &completedInLevel, levelSize,
                       &progress, onProgress,
                       samplesProcessed, totalSamples]()
        {
            if (cancelled_.load()) return;

            auto engineConfig = config.contains(engine->name())
                ? config[engine->name()]
                : config;

            try {
                auto engineResult = engine->execute(storage, engineConfig,
                    [&](float /*pct*/, std::string_view /*stage*/) {
                        // Individual engine progress is tracked
                        // via the overall mechanism below.
                    });

                {
                    std::lock_guard<std::mutex> lock(resultMutex);
                    result.engineResults[engine->name()] = std::move(engineResult);

                    auto& res = result.engineResults[engine->name()];
                    if (!res.success) {
                        cancelled_.store(true);
                        result.success = false;
                        result.error = res.error.value_or(
                            engine->name() + " failed");
                        return;
                    }

                    // Track samples processed.
                    if (res.metrics.contains("total_samples")) {
                        *samplesProcessed +=
                            res.metrics["total_samples"].get<std::uint64_t>();
                    }
                }

                completedInLevel.fetch_add(1);
                {
                    std::lock_guard<std::mutex> lock(resultMutex);
                    progress.enginesCompleted++;
                    progress.overallPercent = totalSamples > 0
                        ? static_cast<float>(*samplesProcessed) /
                          static_cast<float>(totalSamples)
                        : static_cast<float>(progress.enginesCompleted) /
                          static_cast<float>(progress.totalEngines);
                    if (onProgress) onProgress(progress);
                }

            } catch (const std::exception& e) {
                std::lock_guard<std::mutex> lock(resultMutex);
                result.success = false;
                result.error = std::string(engine->name()) + ": " + e.what();
                cancelled_.store(true);
            }
        });
    }

    // Wait for all engines in this level to finish.
    pool_.waitAll();
}

// ── runAll ────────────────────────────────────────────────────────────

AnalysisResult JobScheduler::runAll(
    const std::vector<std::unique_ptr<IAnalysisEngine>>& engines,
    const beamlab::services::storage::IStorageBackend& storage,
    const nlohmann::json& config,
    JobProgressCallback onProgress)
{
    cancelled_.store(false);
    auto startTime = std::chrono::steady_clock::now();

    AnalysisResult result;
    result.success = true;

    auto levels = buildLevels(engines);

    JobProgress progress;
    progress.totalEngines = static_cast<int>(engines.size());
    progress.totalLevels = static_cast<int>(levels.size());

    std::mutex resultMutex;
    std::size_t samplesProcessed = 0;
    auto totalSamples = storage.totalSamples();

    for (std::size_t li = 0; li < levels.size(); ++li) {
        if (cancelled_.load()) break;

        progress.levelCompleted = static_cast<int>(li);
        if (onProgress) onProgress(progress);

        executeLevel(levels[li], storage, config,
                     result, resultMutex,
                     progress, onProgress,
                     &samplesProcessed, totalSamples);
    }

    if (!cancelled_.load() && result.success) {
        progress.levelCompleted = progress.totalLevels;
        progress.overallPercent = 1.0f;
        if (onProgress) onProgress(progress);
    }

    auto endTime = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        endTime - startTime);

    return result;
}

} // namespace beamlab::services::analysis
