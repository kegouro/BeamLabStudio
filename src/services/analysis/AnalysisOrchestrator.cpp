#include "services/analysis/AnalysisOrchestrator.h"
#include "platform/EventBus.h"
#include "services/storage/IStorageBackend.h"

#include <chrono>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;
namespace be = beamlab::platform;

namespace beamlab::services::analysis {

// ── Constructor / Destructor ─────────────────────────────────────────

AnalysisOrchestrator::AnalysisOrchestrator(
    IImporterRegistry* importerRegistry,
    IExporterRegistry* exporterRegistry,
    JobScheduler* scheduler,
    storage::StorageManager* storageManager,
    platform::EventBus* eventBus)
    : importerRegistry_(importerRegistry)
    , exporterRegistry_(exporterRegistry)
    , scheduler_(scheduler)
    , storageManager_(storageManager)
    , eventBus_(eventBus)
{
}

AnalysisOrchestrator::~AnalysisOrchestrator()
{
    cancel();
    if (worker_.joinable()) {
        worker_.join();
    }
}

// ── Speed tracking ──────────────────────────────────────────────────

void AnalysisOrchestrator::updateSpeed(std::uint64_t bytesProcessed)
{
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration<double>(now - stageStart_).count();
    double instant = (elapsed > 0.0)
        ? static_cast<double>(bytesProcessed) / elapsed
        : 0.0;

    // Exponential moving average (α = 0.3).
    constexpr double kAlpha = 0.3;
    smoothedBytesPerSec_ = smoothedBytesPerSec_ == 0.0
        ? instant
        : kAlpha * instant + (1.0 - kAlpha) * smoothedBytesPerSec_;
}

OrchestratorProgress AnalysisOrchestrator::makeProgress(
    const std::string& stage,
    std::uint64_t processed,
    std::uint64_t total) const
{
    OrchestratorProgress p;
    p.stage = stage;
    p.bytesProcessed = processed;
    p.totalBytes = total;
    p.bytesPerSecond = smoothedBytesPerSec_;
    p.percentComplete = (total > 0)
        ? static_cast<int>(100.0 * processed / total)
        : 0;
    p.etaSeconds = (smoothedBytesPerSec_ > 0.0 && total > processed)
        ? static_cast<double>(total - processed) / smoothedBytesPerSec_
        : 0.0;
    return p;
}

// ── Pipeline execution (worker thread) ──────────────────────────────

void AnalysisOrchestrator::executePipeline(
    const std::string& filePath,
    const nlohmann::json& config,
    OrchestratorProgressCallback onProgress,
    OrchestratorDoneCallback onDone)
{
    cancelled_.store(false);
    stageStart_ = std::chrono::steady_clock::now();

    // RAII guard: publish ErrorOccurred + call onDone on any exception.
    struct PipelineGuard {
        AnalysisOrchestrator& self;
        OrchestratorDoneCallback done;
        bool completed = false;

        ~PipelineGuard() {
            if (!completed) {
                auto msg = "Pipeline terminated unexpectedly";
                if (self.eventBus_) {
                    self.eventBus_->publish(be::ErrorOccurred{
                        be::Severity::Error, msg, "AnalysisOrchestrator"});
                }
                if (done) {
                    done(AnalysisResult{false, {}, msg, {}});
                }
            }
        }
    };
    PipelineGuard guard{*this, onDone};

    try {
        // ── Stage 1: Detect ──────────────────────────────────────────
        if (cancelled_.load()) { guard.completed = true; return; }
        if (onProgress) onProgress(makeProgress("Detecting", 0, 0));

        auto* importer = importerRegistry_->detectImporter(filePath);
        if (!importer) {
            auto msg = "No importer found for: " + filePath;
            if (eventBus_) {
                eventBus_->publish(be::ErrorOccurred{
                    be::Severity::Error, msg, "AnalysisOrchestrator"});
            }
            if (onDone) {
                onDone(AnalysisResult{false, {}, msg, {}});
            }
            guard.completed = true;
            return;
        }

        // ── Stage 2: Create storage ──────────────────────────────────
        std::uint64_t fileSize = 0;
        try {
            fileSize = static_cast<std::uint64_t>(fs::file_size(filePath));
        } catch (...) {}

        auto storage = storageManager_->create(fileSize, config);
        if (!storage) {
            auto msg = "Failed to create storage backend";
            if (eventBus_) {
                eventBus_->publish(be::ErrorOccurred{
                    be::Severity::Error, msg, "AnalysisOrchestrator"});
            }
            if (onDone) {
                onDone(AnalysisResult{false, {}, msg, {}});
            }
            guard.completed = true;
            return;
        }

        // ── Stage 3: Import ──────────────────────────────────────────
        if (cancelled_.load()) { guard.completed = true; return; }

        if (eventBus_) {
            eventBus_->publish(be::ImportStarted{filePath, fileSize});
        }

        stageStart_ = std::chrono::steady_clock::now();

        storage::ImportContext ctx{filePath, fileSize};
        storageManager_->importFrom(*importer, ctx, *storage,
            [&](std::uint64_t read, std::uint64_t total) {
                updateSpeed(read);
                auto prog = makeProgress("Importing", read, total);
                if (onProgress) onProgress(prog);

                if (eventBus_) {
                    eventBus_->publish(be::ImportProgress{
                        read, total, prog.etaSeconds});
                }
            });

        if (cancelled_.load()) { guard.completed = true; return; }

        auto totalSamples = storage->totalSamples();

        if (eventBus_) {
            eventBus_->publish(be::ImportCompleted{
                be::DatasetId{1}, totalSamples});
        }

        // ── Stage 4: Analyze ─────────────────────────────────────────
        if (cancelled_.load()) { guard.completed = true; return; }

        if (eventBus_) {
            eventBus_->publish(be::AnalysisStarted{be::DatasetId{1}});
        }

        AnalysisResult analysisResult;

        // Build engine list from config.
        std::vector<std::unique_ptr<IAnalysisEngine>> engines;
        if (config.contains("engines") && config["engines"].is_array()) {
            for (const auto& engCfg : config["engines"]) {
                auto engName = engCfg.get<std::string>();
                // Engines must be registered externally — the scheduler
                // receives them from the caller or a registry.
                // For now, engines are passed through config.
            }
        }

        // Use the injected scheduler with engines from config.
        // (The caller is responsible for registering engines via the
        //  JobScheduler or passing them in the config.)
        {
            auto engineConfig = config.contains("analysis")
                ? config["analysis"]
                : config;

            // Run analysis (engines must be pre-configured on scheduler).
            // If no engines were registered, this is a no-op.
            analysisResult = scheduler_->runAll(
                {}, *storage, engineConfig,
                [&](const JobProgress& jp) {
                    if (eventBus_) {
                        eventBus_->publish(be::AnalysisProgress{
                            static_cast<int>(jp.overallPercent * 100.0f),
                            "Level " + std::to_string(jp.levelCompleted) +
                            "/" + std::to_string(jp.totalLevels)});
                    }
                    OrchestratorProgress prog;
                    prog.stage = "Analyzing";
                    prog.percentComplete =
                        static_cast<int>(jp.overallPercent * 100.0f);
                    if (onProgress) onProgress(prog);
                });
        }

        if (cancelled_.load()) { guard.completed = true; return; }

        if (eventBus_) {
            eventBus_->publish(be::AnalysisCompleted{
                be::DatasetId{1}, ""});
        }

        // ── Stage 5: Export (optional) ───────────────────────────────
        std::vector<std::string> exportFormats;
        if (config.contains("auto_export_formats") &&
            config["auto_export_formats"].is_array()) {
            for (const auto& fmt : config["auto_export_formats"]) {
                exportFormats.push_back(fmt.get<std::string>());
            }
        }

        if (!exportFormats.empty() && analysisResult.success) {
            auto outDir = config.value("output_dir",
                (fs::temp_directory_path() / "beamlab_export").string());
            fs::create_directories(outDir);

            for (const auto& fmt : exportFormats) {
                if (cancelled_.load()) break;
                if (eventBus_) {
                    eventBus_->publish(be::ExportStarted{fmt});
                }
            }

            exporterRegistry_->exportAll(
                *storage, analysisResult, outDir, exportFormats);
        }

        // ── Done ─────────────────────────────────────────────────────
        if (onDone) {
            onDone(analysisResult);
        }

        guard.completed = true;

    } catch (const std::exception& e) {
        auto msg = std::string(e.what());
        if (eventBus_) {
            eventBus_->publish(be::ErrorOccurred{
                be::Severity::Error, msg, "AnalysisOrchestrator"});
        }
        if (onDone) {
            onDone(AnalysisResult{false, {}, msg, {}});
        }
        guard.completed = true;
    }
}

// ── Public API ────────────────────────────────────────────────────────

void AnalysisOrchestrator::run(
    const std::string& filePath,
    const nlohmann::json& config,
    OrchestratorProgressCallback onProgress,
    OrchestratorDoneCallback onDone)
{
    if (worker_.joinable()) {
        if (onDone) {
            onDone(AnalysisResult{false, {},
                "Analysis already in progress", {}});
        }
        return;
    }

    worker_ = std::thread([this, filePath, config,
                           onProgress = std::move(onProgress),
                           onDone = std::move(onDone)]() {
        executePipeline(filePath, config, onProgress, onDone);
    });
    worker_.detach();
}

} // namespace beamlab::services::analysis
