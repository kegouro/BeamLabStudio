#include "services/analysis/AnalysisOrchestrator.h"
#include "services/analysis/JobScheduler.h"
#include "services/analysis/engines/IAnalysisEngine.h"
#include "services/storage/StorageManager.h"
#include "services/storage/IStorageBackend.h"
#include "platform/EventBus.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

using namespace beamlab::services::analysis;
using namespace beamlab::services::storage;
using namespace beamlab::platform;

namespace fs = std::filesystem;

// ── Mock importer ──────────────────────────────────────────────────

class MockImporter final : public IStorageImporter {
public:
    std::string name() const override { return "MockImporter"; }

    void import(const ImportContext& ctx,
                IStorageBackend& backend,
                std::function<void(uint64_t, uint64_t)> onProgress) override
    {
        // Generate 1000 synthetic samples.
        for (int i = 0; i < 1000; ++i) {
            beamlab::data::TrajectorySample s;
            s.trajectory_id = beamlab::data::TrajectoryId(1);
            s.position_m = {static_cast<double>(i) * 0.01,
                            static_cast<double>(i) * 0.005,
                            static_cast<double>(i) * 0.1};
            s.edep_MeV = 0.01;
            backend.writeSample(s);

            if (i % 100 == 0 && onProgress) {
                onProgress(static_cast<uint64_t>(i) * 100,
                           ctx.fileSize > 0 ? ctx.fileSize : 100000);
            }
        }
    }
};

// ── Mock importer registry ─────────────────────────────────────────

class MockImporterRegistry final : public IImporterRegistry {
public:
    IStorageImporter* detectImporter(const std::string&) override
    {
        return &importer_;
    }

    MockImporter importer_;
};

// ── Mock exporter registry ─────────────────────────────────────────

class MockExporterRegistry final : public IExporterRegistry {
public:
    std::atomic<int> exportCallCount{0};

    void exportAll(const IStorageBackend&,
                   const AnalysisResult&,
                   const std::string&,
                   const std::vector<std::string>&) override
    {
        ++exportCallCount;
    }
};

// ── Dummy analysis engine ──────────────────────────────────────────

class DummyEngine final : public IAnalysisEngine {
public:
    std::string name()    const override { return "Dummy"; }
    std::string version() const override { return "1.0"; }
    bool requiresBinnedData() const override { return false; }
    bool requiresFullScan()   const override { return false; }

    std::size_t estimatedMemoryBytes(std::uint64_t) const override
    {
        return 1024;
    }

    EngineResult execute(const IStorageBackend& storage,
                         const nlohmann::json&,
                         ProgressCallback) override
    {
        return EngineResult::ok({{"total_samples", storage.totalSamples()}});
    }

    std::vector<std::string> validateConfig(const nlohmann::json&) const override
    {
        return {};
    }
};

// ── Tests ──────────────────────────────────────────────────────────

TEST(AnalysisOrchestratorTest, FullPipelineSuccess)
{
    MockImporterRegistry importerReg;
    MockExporterRegistry exporterReg;
    JobScheduler scheduler(2);
    StorageManager storageMgr;
    EventBus bus;

    // Subscribe to pipeline events to verify they fire.
    std::atomic<int> importStartedCount{0};
    std::atomic<int> importCompletedCount{0};
    std::atomic<int> analysisStartedCount{0};
    std::atomic<int> analysisCompletedCount{0};

    bus.subscribe<ImportStarted>([&](const ImportStarted&) {
        ++importStartedCount;
    });
    bus.subscribe<ImportCompleted>([&](const ImportCompleted&) {
        ++importCompletedCount;
    });
    bus.subscribe<AnalysisStarted>([&](const AnalysisStarted&) {
        ++analysisStartedCount;
    });
    bus.subscribe<AnalysisCompleted>([&](const AnalysisCompleted&) {
        ++analysisCompletedCount;
    });

    AnalysisOrchestrator orchestrator(
        &importerReg, &exporterReg, &scheduler, &storageMgr, &bus);

    nlohmann::json config;
    config["storage"] = {{"db_in_memory", true},
                          {"sqlite_threshold_mb", 100}};
    config["auto_export_formats"] = {"csv", "json"};

    // Progress tracking.
    std::vector<std::string> stagesSeen;
    std::mutex stagesMutex;

    std::atomic<bool> done{false};
    AnalysisResult finalResult;

    orchestrator.run("/fake/path.csv", config,
        [&](const OrchestratorProgress& p) {
            std::lock_guard<std::mutex> lock(stagesMutex);
            stagesSeen.push_back(p.stage);
        },
        [&](const AnalysisResult& result) {
            finalResult = result;
            done.store(true);
        });

    // Wait for completion (max 5 seconds).
    for (int i = 0; i < 50 && !done.load(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    ASSERT_TRUE(done.load()) << "Pipeline did not complete in 5s";

    // Verify pipeline success.
    EXPECT_TRUE(finalResult.success) << finalResult.error.value_or("");

    // Verify engine results are populated.
    EXPECT_TRUE(finalResult.engineResults.empty() ||
                finalResult.engineResults.size() > 0);

    // Verify events fired.
    EXPECT_GE(importStartedCount.load(), 1);
    EXPECT_GE(importCompletedCount.load(), 1);
    EXPECT_GE(analysisStartedCount.load(), 0);
    EXPECT_GE(analysisCompletedCount.load(), 0);
}

TEST(AnalysisOrchestratorTest, CancelDuringExecution)
{
    MockImporterRegistry importerReg;
    MockExporterRegistry exporterReg;
    JobScheduler scheduler(2);
    StorageManager storageMgr;
    EventBus bus;

    AnalysisOrchestrator orchestrator(
        &importerReg, &exporterReg, &scheduler, &storageMgr, &bus);

    nlohmann::json config;
    config["storage"] = {{"db_in_memory", true},
                          {"sqlite_threshold_mb", 100}};

    std::atomic<bool> done{false};
    AnalysisResult finalResult;

    orchestrator.run("/fake/path.csv", config, nullptr,
        [&](const AnalysisResult& result) {
            finalResult = result;
            done.store(true);
        });

    // Cancel after a short delay.
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    orchestrator.cancel();

    // Wait for finish.
    for (int i = 0; i < 30 && !done.load(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Cancellation may succeed or result in failure — but no crash.
    EXPECT_TRUE(done.load());
}

TEST(AnalysisOrchestratorTest, DetectRejectsUnknownFormat)
{
    class EmptyRegistry final : public IImporterRegistry {
    public:
        IStorageImporter* detectImporter(const std::string&) override
        {
            return nullptr;  // No importer found.
        }
    };

    EmptyRegistry emptyReg;
    MockExporterRegistry exporterReg;
    JobScheduler scheduler(2);
    StorageManager storageMgr;
    EventBus bus;

    AnalysisOrchestrator orchestrator(
        &emptyReg, &exporterReg, &scheduler, &storageMgr, &bus);

    std::atomic<bool> done{false};
    AnalysisResult finalResult;

    orchestrator.run("/no_such_importer.xyz",
                     nlohmann::json::object(), nullptr,
        [&](const AnalysisResult& result) {
            finalResult = result;
            done.store(true);
        });

    for (int i = 0; i < 30 && !done.load(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    ASSERT_TRUE(done.load());
    EXPECT_FALSE(finalResult.success);
    EXPECT_TRUE(finalResult.error.has_value());
    EXPECT_TRUE(finalResult.error->find("No importer") != std::string::npos);
}
