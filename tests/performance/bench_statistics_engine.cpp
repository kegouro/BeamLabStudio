#include "services/analysis/engines/FrameStatisticsPlugin.h"
#include "services/analysis/ThreadPool.h"
#include "services/storage/SqliteBackend.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <iostream>
#include <random>

namespace fs = std::filesystem;
using namespace beamlab::services::analysis;
using namespace beamlab::services::storage;
using namespace beamlab::data;

// ── Helper ─────────────────────────────────────────────────────────

static void seedStorage(SqliteBackend& backend, uint64_t nSamples)
{
    std::mt19937_64 rng(42);
    std::uniform_real_distribution<double> pos(-5.0, 5.0);

    backend.beginWriteBatch();
    for (uint64_t i = 0; i < nSamples; ++i) {
        TrajectorySample s;
        s.trajectory_id = TrajectoryId(static_cast<uint64_t>(i % 100 + 1));
        s.position_m = {pos(rng), pos(rng),
                        static_cast<double>(i) / static_cast<double>(nSamples - 1) * 10.0};
        s.edep_MeV = 0.01;
        s.kinE_MeV = 100.0;
        backend.writeSample(s);
    }
    backend.endWriteBatch();
    backend.finalizeIndices();
}

// ── Benchmarks ─────────────────────────────────────────────────────

TEST(StatisticsEngineBenchmark, FrameStatistics_500k_Samples)
{
    auto dbPath = (fs::temp_directory_path() / "bench_fs_500k.db").string();
    SqliteBackend backend(dbPath);
    seedStorage(backend, 500'000);

    nlohmann::json config;
    config["axis_frame"] = {
        {"origin",       {{"x", 0}, {"y", 0}, {"z", 0}}},
        {"longitudinal", {{"x", 0}, {"y", 0}, {"z", 1}}},
        {"transverse_u", {{"x", 1}, {"y", 0}, {"z", 0}}},
        {"transverse_v", {{"x", 0}, {"y", 1}, {"z", 0}}},
    };
    config["bin_count"] = 50;
    config["batch_size"] = 100000;

    FrameStatisticsPlugin engine;

    auto start = std::chrono::high_resolution_clock::now();
    auto result = engine.execute(backend, config, nullptr);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - start).count();

    EXPECT_TRUE(result.success);
    auto frames = result.metrics["frames_data"].size();

    std::cout << "[Benchmark] FrameStats 500k samples:\n"
              << "  Time:   " << ms << " ms\n"
              << "  Frames: " << frames << "\n"
              << "  Rate:   "
              << (500'000.0 * 1000.0 / (ms > 0 ? ms : 1)) << " samples/s\n";

    EXPECT_GT(frames, 0u);
    EXPECT_LT(ms, 10000) << "500k stats took " << ms << "ms (target: <10s)";

    fs::remove(dbPath);
}

TEST(StatisticsEngineBenchmark, FrameStatistics_1M_Samples)
{
    auto dbPath = (fs::temp_directory_path() / "bench_fs_1M.db").string();
    SqliteBackend backend(dbPath);
    seedStorage(backend, 1'000'000);

    nlohmann::json config;
    config["axis_frame"] = {
        {{"x", 0}, {"y", 0}, {"z", 0}},
        {{"x", 0}, {"y", 0}, {"z", 1}},
        {{"x", 1}, {"y", 0}, {"z", 0}},
        {{"x", 0}, {"y", 1}, {"z", 0}},
    };
    config["bin_count"] = 50;
    config["batch_size"] = 100000;

    FrameStatisticsPlugin engine;

    auto start = std::chrono::high_resolution_clock::now();
    engine.execute(backend, config, nullptr);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - start).count();

    std::cout << "[Benchmark] FrameStats 1M samples: " << ms << " ms\n";
    EXPECT_LT(ms, 20000) << "1M stats took " << ms << "ms";

    fs::remove(dbPath);
}

TEST(StatisticsEngineBenchmark, ParallelVsSerial)
{
    auto dbPath = (fs::temp_directory_path() / "bench_parallel.db").string();
    SqliteBackend backend(dbPath);
    seedStorage(backend, 1'000'000);

    // Just verify the engine doesn't crash in parallel mode.
    nlohmann::json config;
    config["axis_frame"] = {
        {{"x", 0}, {"y", 0}, {"z", 0}},
        {{"x", 0}, {"y", 0}, {"z", 1}},
        {{"x", 1}, {"y", 0}, {"z", 0}},
        {{"x", 0}, {"y", 1}, {"z", 0}},
    };
    config["bin_count"] = 20;
    config["batch_size"] = 100000;

    FrameStatisticsPlugin engine;
    auto result = engine.execute(backend, config, nullptr);
    EXPECT_TRUE(result.success);

    fs::remove(dbPath);
}
