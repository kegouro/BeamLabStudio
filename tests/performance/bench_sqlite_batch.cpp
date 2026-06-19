#include "services/storage/SqliteBackend.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <iostream>
#include <random>

namespace fs = std::filesystem;
using namespace beamlab::services::storage;
using namespace beamlab::data;

// ── Helper ─────────────────────────────────────────────────────────

static TrajectorySample generateRandomSample(std::mt19937_64& rng)
{
    std::uniform_real_distribution<double> pos(-10.0, 10.0);
    std::uniform_real_distribution<double> edep(0.001, 1.0);

    TrajectorySample s;
    s.trajectory_id = TrajectoryId(
        static_cast<uint64_t>(std::uniform_int_distribution<int>(1, 100)(rng)));
    s.position_m = {pos(rng), pos(rng), pos(rng)};
    s.edep_MeV = edep(rng);
    s.kinE_MeV = 100.0;
    return s;
}

// ── Benchmarks ─────────────────────────────────────────────────────

TEST(SqliteBatchBenchmark, Insert_100k_Rows)
{
    auto dbPath = (fs::temp_directory_path() / "bench_100k.db").string();
    SqliteBackend backend(dbPath);

    std::mt19937_64 rng(42);

    backend.beginWriteBatch();
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 100'000; ++i) {
        backend.writeSample(generateRandomSample(rng));
    }

    backend.endWriteBatch();
    auto elapsed = std::chrono::high_resolution_clock::now() - start;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

    auto samples = backend.totalSamples();

    std::cout << "[Benchmark] 100k row insert:\n"
              << "  Time:   " << ms << " ms\n"
              << "  Samples: " << samples << "\n"
              << "  Rate:   " << (samples * 1000.0 / (ms > 0 ? ms : 1))
              << " samples/s\n";

    EXPECT_EQ(samples, 100'000u);
    EXPECT_LT(ms, 3000) << "100k inserts took " << ms << "ms";

    fs::remove(dbPath);
}

TEST(SqliteBatchBenchmark, Insert_1M_Rows)
{
    auto dbPath = (fs::temp_directory_path() / "bench_1M.db").string();
    SqliteBackend backend(dbPath);

    std::mt19937_64 rng(42);

    backend.beginWriteBatch();
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 1'000'000; ++i) {
        backend.writeSample(generateRandomSample(rng));
    }

    backend.endWriteBatch();
    auto elapsed = std::chrono::high_resolution_clock::now() - start;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

    std::cout << "[Benchmark] 1M row insert:\n"
              << "  Time: " << ms << " ms  ("
              << (1'000'000.0 * 1000.0 / (ms > 0 ? ms : 1)) << " samples/s)\n";

    EXPECT_EQ(backend.totalSamples(), 1'000'000u);
    EXPECT_LT(ms, 15000) << "1M inserts took " << ms << "ms (target: <15s)";

    fs::remove(dbPath);
}

TEST(SqliteBatchBenchmark, ReadBatchPerformance)
{
    auto dbPath = (fs::temp_directory_path() / "bench_read.db").string();
    SqliteBackend backend(dbPath);
    backend.setBatchSize(100000);

    // Seed 500k rows.
    std::mt19937_64 rng(42);
    backend.beginWriteBatch();
    for (int i = 0; i < 500'000; ++i)
        backend.writeSample(generateRandomSample(rng));
    backend.endWriteBatch();
    backend.finalizeIndices();

    // Read 100 batches of 10k.
    auto start = std::chrono::high_resolution_clock::now();
    uint64_t total = 0;
    for (int i = 0; i < 100; ++i) {
        auto batch = backend.readBatch(static_cast<uint64_t>(i) * 10000, 10000);
        total += batch.count;
    }
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - start).count();

    std::cout << "[Benchmark] 100 batch reads (10k each): "
              << ms << " ms  (" << total << " samples)\n";

    EXPECT_LT(ms, 1000) << "100 batch reads took " << ms << "ms";

    fs::remove(dbPath);
}
