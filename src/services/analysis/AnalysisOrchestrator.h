#pragma once

#include "services/analysis/JobScheduler.h"
#include "services/storage/StorageManager.h"

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <thread>

namespace beamlab::platform {
class EventBus;
} // namespace beamlab::platform

namespace beamlab::services::analysis {

// ── Progress / estimate helpers ──────────────────────────────────────

struct OrchestratorProgress {
    std::string stage;          // "Detecting", "Importing", "Analyzing", "Exporting"
    int percentComplete{0};     // 0–100
    double bytesPerSecond{0.0};
    double etaSeconds{0.0};
    std::uint64_t bytesProcessed{0};
    std::uint64_t totalBytes{0};
};

using OrchestratorProgressCallback =
    std::function<void(const OrchestratorProgress&)>;

using OrchestratorDoneCallback =
    std::function<void(const AnalysisResult&)>;

// ── Minimal registry interfaces expected by the orchestrator ────────

namespace storage {
class IStorageImporter;
} // namespace storage

class IImporterRegistry {
public:
    virtual ~IImporterRegistry() = default;
    virtual storage::IStorageImporter* detectImporter(
        const std::string& filePath) = 0;
};

class IExporterRegistry {
public:
    virtual ~IExporterRegistry() = default;
    virtual void exportAll(
        const IStorageBackend& storage,
        const AnalysisResult& result,
        const std::string& outputDir,
        const std::vector<std::string>& formats) = 0;
};

// ── AnalysisOrchestrator ─────────────────────────────────────────────

class AnalysisOrchestrator {
public:
    AnalysisOrchestrator(
        IImporterRegistry* importerRegistry,
        IExporterRegistry* exporterRegistry,
        JobScheduler* scheduler,
        storage::StorageManager* storageManager,
        platform::EventBus* eventBus);

    ~AnalysisOrchestrator();

    AnalysisOrchestrator(const AnalysisOrchestrator&) = delete;
    AnalysisOrchestrator& operator=(const AnalysisOrchestrator&) = delete;

    /// Start analysis pipeline in a background thread.
    /// Returns immediately; result delivered via onDone.
    void run(
        const std::string& filePath,
        const nlohmann::json& config,
        OrchestratorProgressCallback onProgress = nullptr,
        OrchestratorDoneCallback onDone = nullptr);

    /// Request graceful cancellation.
    void cancel() { cancelled_.store(true); scheduler_->cancel(); }
    bool isCancelled() const { return cancelled_.load(); }

    /// Whether a run is currently in progress.
    bool isRunning() const { return worker_.joinable(); }

private:
    IImporterRegistry* importerRegistry_;
    IExporterRegistry* exporterRegistry_;
    JobScheduler* scheduler_;
    storage::StorageManager* storageManager_;
    platform::EventBus* eventBus_;

    std::thread worker_;
    std::atomic<bool> cancelled_{false};

    // Speed tracking for ETA.
    mutable std::mutex speedMutex_;
    std::chrono::steady_clock::time_point stageStart_;
    std::uint64_t stageBytesProcessed_{0};
    double smoothedBytesPerSec_{0.0};

    void updateSpeed(std::uint64_t bytesProcessed);
    OrchestratorProgress makeProgress(const std::string& stage,
                                       std::uint64_t processed,
                                       std::uint64_t total) const;

    /// Core pipeline (runs on worker thread).
    void executePipeline(
        const std::string& filePath,
        const nlohmann::json& config,
        OrchestratorProgressCallback onProgress,
        OrchestratorDoneCallback onDone);
};

} // namespace beamlab::services::analysis
