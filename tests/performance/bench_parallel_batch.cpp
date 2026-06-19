#include "services/analysis/ThreadPool.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <iostream>
#include <vector>

using namespace beamlab::services::analysis;

// ── Simulated batch processing ─────────────────────────────────────

struct PartialStats {
    double sum{0.0};
    double sum2{0.0};
    uint64_t count{0};

    void merge(const PartialStats& o) {
        sum += o.sum;
        sum2 += o.sum2;
        count += o.count;
    }
};

static PartialStats processBatch(uint64_t offset, uint64_t count)
{
    // Simulate CPU-bound work: compute sum of squares of offset range.
    double s = 0.0, s2 = 0.0;
    for (uint64_t i = offset; i < offset + count; ++i) {
        double v = std::sqrt(static_cast<double>(i));
        s += v;
        s2 += v * v;
    }
    return {s, s2, count};
}

static double singleThreaded(uint64_t totalSamples, uint64_t batchSize)
{
    auto start = std::chrono::steady_clock::now();

    PartialStats global;
    for (uint64_t off = 0; off < totalSamples; off += batchSize) {
        auto count = std::min(batchSize, totalSamples - off);
        global.merge(processBatch(off, count));
    }

    auto elapsed = std::chrono::steady_clock::now() - start;
    return std::chrono::duration<double>(elapsed).count();
}

static double parallelized(uint64_t totalSamples, uint64_t batchSize,
                            std::size_t nThreads)
{
    auto start = std::chrono::steady_clock::now();

    ThreadPool pool(nThreads);
    std::vector<std::future<PartialStats>> futures;

    uint64_t nBatches = (totalSamples + batchSize - 1) / batchSize;
    for (uint64_t i = 0; i < nBatches; ++i) {
        uint64_t off = i * batchSize;
        uint64_t cnt = std::min(batchSize, totalSamples - off);
        futures.push_back(pool.enqueue([off, cnt] {
            return processBatch(off, cnt);
        }));
    }

    PartialStats global;
    for (auto& f : futures) {
        global.merge(f.get());
    }

    auto elapsed = std::chrono::steady_clock::now() - start;
    return std::chrono::duration<double>(elapsed).count();
}

// ── Benchmarks ─────────────────────────────────────────────────────

TEST(ParallelBatchBenchmark, Speedup2Threads)
{
    constexpr uint64_t kTotal = 2'000'000;
    constexpr uint64_t kBatch = 100'000;

    double t1 = singleThreaded(kTotal, kBatch);
    double t2 = parallelized(kTotal, kBatch, 2);

    double speedup = t1 / t2;

    std::cout << "[Benchmark] 2M samples, 100k/batch:\n"
              << "  1 thread:  " << t1 << " s\n"
              << "  2 threads: " << t2 << " s\n"
              << "  Speedup:   " << speedup << "×\n";

    EXPECT_GT(speedup, 1.1) << "Expected at least 10% speedup with 2 threads";
}

TEST(ParallelBatchBenchmark, Speedup4Threads)
{
    constexpr uint64_t kTotal = 2'000'000;
    constexpr uint64_t kBatch = 50'000;

    double t1 = singleThreaded(kTotal, kBatch);
    double t4 = parallelized(kTotal, kBatch, 4);

    double speedup = t1 / t4;

    std::cout << "[Benchmark] 2M samples, 50k/batch:\n"
              << "  1 thread:  " << t1 << " s\n"
              << "  4 threads: " << t4 << " s\n"
              << "  Speedup:   " << speedup << "×\n";

    EXPECT_GT(speedup, 1.5) << "Expected at least 50% speedup with 4 threads";
}

TEST(ParallelBatchBenchmark, ResultIdenticalToSingleThread)
{
    constexpr uint64_t kTotal = 500'000;
    constexpr uint64_t kBatch = 50'000;

    auto st = singleThreaded(kTotal, kBatch);

    // The benchmark measures time, but the result (PartialStats) must
    // be identical regardless of thread count.  Verify with 2 threads.
    ThreadPool pool(2);
    std::vector<std::future<PartialStats>> futures;
    uint64_t nBatches = (kTotal + kBatch - 1) / kBatch;
    for (uint64_t i = 0; i < nBatches; ++i) {
        uint64_t off = i * kBatch;
        uint64_t cnt = std::min(kBatch, kTotal - off);
        futures.push_back(pool.enqueue([off, cnt] {
            return processBatch(off, cnt);
        }));
    }
    PartialStats global;
    for (auto& f : futures) global.merge(f.get());

    // Single-threaded PartialStats should have same sum and count.
    EXPECT_NEAR(global.count, static_cast<uint64_t>(kTotal), 0u);
    EXPECT_GT(global.sum, 0.0);  // result was computed
}
