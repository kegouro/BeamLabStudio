#pragma once

#include "services/analysis/engines/IAnalysisEngine.h"
#include "services/analysis/ThreadPool.h"
#include "services/storage/IStorageBackend.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace beamlab::services::analysis {

/// Overall result assembled after running a set of engines.
struct AnalysisResult {
    bool success{false};
    std::unordered_map<std::string, EngineResult> engineResults;
    std::optional<std::string> error;
    std::chrono::milliseconds duration{0};
};

/// Progress reported during multi-engine execution.
struct JobProgress {
    int levelCompleted{0};
    int totalLevels{0};
    int enginesCompleted{0};
    int totalEngines{0};
    float overallPercent{0.0f};
};

/// Signature for overall progress callbacks.
using JobProgressCallback = std::function<void(const JobProgress&)>;

/// Orchestrates a set of IAnalysisEngine instances, running independent
/// engines in parallel while respecting structural dependencies.
///
/// ## Dependency model
///
/// Engines are grouped into execution levels:
///
///   Level 1 — engines that do NOT require binned data
///             (work directly on IStorageBackend raw samples).
///   Level 2 — engines that REQUIRE binned data
///             (depend on Level 1 having produced frame statistics).
///
/// All engines within the same level run concurrently via ThreadPool.
/// Level N+1 starts only after Level N completes.
///
class JobScheduler {
public:
    /// \param threadCount  Worker threads (default = hardware concurrency).
    explicit JobScheduler(std::size_t threadCount = 0);

    JobScheduler(const JobScheduler&) = delete;
    JobScheduler& operator=(const JobScheduler&) = delete;

    /// Run all engines against the storage backend.
    ///
    /// \param engines     Engines to execute (owned by caller).
    /// \param storage     Backend containing imported samples.
    /// \param config      Per-engine configuration (top-level keys are
    ///                    engine names or shared settings).
    /// \param onProgress  Optional overall-progress callback.
    AnalysisResult runAll(
        const std::vector<std::unique_ptr<IAnalysisEngine>>& engines,
        const beamlab::services::storage::IStorageBackend& storage,
        const nlohmann::json& config,
        JobProgressCallback onProgress = nullptr);

    /// Cancel all engines.  Running engines check their cancellation
    /// flag between batches; queued engines are never started.
    void cancel() { cancelled_.store(true); }
    bool isCancelled() const { return cancelled_.load(); }

private:
    ThreadPool pool_;
    std::atomic<bool> cancelled_{false};

    /// Build execution levels.  Level 0 = no deps, Level 1 = deps met.
    std::vector<std::vector<IAnalysisEngine*>> buildLevels(
        const std::vector<std::unique_ptr<IAnalysisEngine>>& engines) const;

    /// Execute a single level in parallel.
    void executeLevel(
        const std::vector<IAnalysisEngine*>& level,
        const beamlab::services::storage::IStorageBackend& storage,
        const nlohmann::json& config,
        AnalysisResult& result,
        std::mutex& resultMutex,
        JobProgress& progress,
        JobProgressCallback onProgress,
        std::size_t* samplesProcessed,
        std::size_t totalSamples);
};

} // namespace beamlab::services::analysis
