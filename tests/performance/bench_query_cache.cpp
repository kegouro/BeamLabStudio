#include "services/storage/QueryCache.h"

#include <gtest/gtest.h>

#include <chrono>
#include <string>
#include <vector>

using namespace beamlab::services::storage;

TEST(QueryCacheBenchmark, OneMillionGets)
{
    QueryCache cache(1000);

    // Populate 10 keys with trivial data.
    for (int i = 0; i < 10; ++i) {
        cache.put("key_" + std::to_string(i), i);
    }

    // Measure 1 million get() calls.
    auto start = std::chrono::steady_clock::now();

    int totalHits = 0;
    for (int i = 0; i < 1'000'000; ++i) {
        auto key = "key_" + std::to_string(i % 10);
        if (cache.get<int>(key).has_value())
            ++totalHits;
    }

    auto elapsed = std::chrono::steady_clock::now() - start;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    auto ns_per_get = std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count() / 1'000'000;

    std::cout << "\n[QueryCache Benchmark] 1,000,000 gets:\n"
              << "  Time:   " << ms << " ms\n"
              << "  Per-call: " << ns_per_get << " ns/get\n"
              << "  Hits:   " << totalHits << " / 1,000,000\n"
              << "  Throughput: "
              << (ms > 0 ? 1'000'000.0 / (ms * 1.0) * 1000.0 : 0.0)
              << " gets/sec\n";

    // Must be fast (< 200ms for 1M gets ≈ 200ns/get).
    EXPECT_LT(ms, 500) << "1M gets took " << ms << "ms (target: < 500ms)";
}

TEST(QueryCacheBenchmark, MixedPutGet500k)
{
    QueryCache cache(2000);

    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < 500'000; ++i) {
        std::string key = "item_" + std::to_string(i % 2000);
        cache.put(key, i, std::chrono::hours(1));
        auto val = cache.get<int>(key);
        EXPECT_TRUE(val.has_value());
    }

    auto elapsed = std::chrono::steady_clock::now() - start;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

    auto stats = cache.stats();

    std::cout << "[QueryCache] 500k put+get pairs:\n"
              << "  Time:   " << ms << " ms\n"
              << "  HitRate: " << stats.hitRate * 100.0 << "%\n"
              << "  Entries: " << stats.currentEntries << "\n";

    EXPECT_LT(ms, 1000) << "500k put+get took " << ms << "ms (target: < 1000ms)";
    EXPECT_GT(stats.hitRate, 0.90) << "Hit rate too low: " << stats.hitRate;
}

TEST(QueryCacheBenchmark, LruPerformance)
{
    QueryCache cache(10);

    auto start = std::chrono::steady_clock::now();

    // Insert 1M entries into a 10-entry cache.  This stresses eviction.
    for (int i = 0; i < 1'000'000; ++i) {
        cache.put("k" + std::to_string(i % 100), i, std::chrono::hours(1));
    }

    auto elapsed = std::chrono::steady_clock::now() - start;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

    std::cout << "[QueryCache] 1M puts into 10-entry LRU:\n"
              << "  Time: " << ms << " ms\n"
              << "  Entries: " << cache.size() << " / " << cache.maxSize() << "\n";

    EXPECT_EQ(cache.size(), 10u);
    EXPECT_LT(ms, 500) << "1M puts into LRU(10) took " << ms << "ms";
}
