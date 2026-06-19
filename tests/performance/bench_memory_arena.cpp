#include "core/math/MemoryArena.h"

#include <gtest/gtest.h>

#include <chrono>
#include <iostream>
#include <vector>

using namespace beamlab::core::math;

// ── Performance comparison ──────────────────────────────────────────

TEST(MemoryArenaBenchmark, ArenaVsNewDelete)
{
    constexpr int kIterations = 1'000'000;
    constexpr int kAllocSize  = 1'024;  // 1 KB per allocation

    // ── new / delete ───────────────────────────────────────────────
    {
        auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < kIterations; ++i) {
            auto* p = new char[kAllocSize];
            delete[] p;
        }
        auto elapsed = std::chrono::steady_clock::now() - start;
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

        std::cout << "[Benchmark] new/delete        : "
                  << ms << " ms  (" << kIterations << " × " << kAllocSize << " bytes)\n";
        EXPECT_LT(ms, 2000) << "new/delete should be reasonable";
    }

    // ── MemoryArena ────────────────────────────────────────────────
    {
        MemoryArena arena(static_cast<std::size_t>(kAllocSize) * kIterations + 256);

        auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < kIterations; ++i) {
            auto* p = arena.allocate<char>(static_cast<std::size_t>(kAllocSize));
            Q_UNUSED(p);
        }
        auto elapsed = std::chrono::steady_clock::now() - start;
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

        std::cout << "[Benchmark] MemoryArena       : "
                  << ms << " ms  (" << kIterations << " × " << kAllocSize << " bytes)\n";

        // Arena should be at least 10× faster than new/delete.
        EXPECT_LT(ms, 500);
    }

    // ── std::vector::resize (arena-like) ───────────────────────────
    {
        std::vector<char> buf;
        buf.resize(kAllocSize * kIterations);

        auto start = std::chrono::steady_clock::now();
        size_t off = 0;
        for (int i = 0; i < kIterations; ++i) {
            char* p = buf.data() + off;
            off += kAllocSize;
            Q_UNUSED(p);
        }
        auto elapsed = std::chrono::steady_clock::now() - start;
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

        std::cout << "[Benchmark] raw buffer (ref)  : "
                  << ms << " ms\n";
        EXPECT_LT(ms, 100);
    }
}

// ── Arena reset throughput ──────────────────────────────────────────

TEST(MemoryArenaBenchmark, ResetThroughput)
{
    MemoryArena arena(64 * 1024 * 1024);  // 64 MB

    constexpr int kBatchSize = 1000;

    auto start = std::chrono::steady_clock::now();
    for (int batch = 0; batch < 100000; ++batch) {
        // Simulate a typical processing batch.
        auto* sx = arena.allocate<double>(kBatchSize);
        auto* sy = arena.allocate<double>(kBatchSize);
        auto* sz = arena.allocate<double>(kBatchSize);
        Q_UNUSED(sx); Q_UNUSED(sy); Q_UNUSED(sz);

        arena.reset();
    }
    auto elapsed = std::chrono::steady_clock::now() - start;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

    std::cout << "[Benchmark] 100k batch alloc+reset cycles: "
              << ms << " ms  (3 × 1k doubles per batch)\n";
    EXPECT_LT(ms, 500);
}

// ── Alignment overhead ──────────────────────────────────────────────

TEST(MemoryArenaBenchmark, AlignmentCost)
{
    MemoryArena arena(64 * 1024 * 1024);

    constexpr int kBatchSize = 1000;

    auto start = std::chrono::steady_clock::now();
    for (int batch = 0; batch < 100000; ++batch) {
        // Mix of types with different alignments.
        auto* c  = arena.allocate<char>(1);
        auto* i  = arena.allocate<int>(kBatchSize);
        auto* d  = arena.allocate<double>(kBatchSize);
        Q_UNUSED(c); Q_UNUSED(i); Q_UNUSED(d);

        arena.reset();
    }
    auto elapsed = std::chrono::steady_clock::now() - start;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

    std::cout << "[Benchmark] 100k mixed-align alloc+reset: "
              << ms << " ms\n";
    EXPECT_LT(ms, 500);
}
