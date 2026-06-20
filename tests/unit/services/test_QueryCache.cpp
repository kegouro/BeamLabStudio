#include "services/storage/QueryCache.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

using namespace beamlab::services::storage;

// ── Test fixture ────────────────────────────────────────────────────

class QueryCacheTest : public ::testing::Test {
protected:
    QueryCache cache{100};
};

// ── Basic hit/miss ──────────────────────────────────────────────────

TEST_F(QueryCacheTest, MissReturnsNullopt)
{
    auto result = cache.get<int>("nonexistent");
    EXPECT_FALSE(result.has_value());
}

TEST_F(QueryCacheTest, PutThenGetReturnsValue)
{
    cache.put("answer", 42);
    auto result = cache.get<int>("answer");

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 42);
}

TEST_F(QueryCacheTest, PutOverwriteGetsLatest)
{
    cache.put("key", 10);
    cache.put("key", 20);
    auto result = cache.get<int>("key");

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 20);
}

TEST_F(QueryCacheTest, DifferentTypesAreIsolated)
{
    cache.put("number", 42);
    cache.put("text", std::string("hello"));

    EXPECT_EQ(*cache.get<int>("number"), 42);
    EXPECT_EQ(*cache.get<std::string>("text"), "hello");
}

// ── TTL ─────────────────────────────────────────────────────────────

TEST_F(QueryCacheTest, ExpiredTTLReturnsNullopt)
{
    cache.put("short_lived", 99, std::chrono::seconds(0));
    // The TTL of 0 seconds means it's already expired.

    auto result = cache.get<int>("short_lived");
    EXPECT_FALSE(result.has_value());
}

TEST_F(QueryCacheTest, NonExpiredTTLSurvives)
{
    cache.put("long_lived", 99, std::chrono::hours(1));

    auto result = cache.get<int>("long_lived");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 99);
}

// ── Statistics ──────────────────────────────────────────────────────

TEST_F(QueryCacheTest, StatsCountHits)
{
    cache.put("a", 1);
    cache.put("b", 2);

    cache.get<int>("a");   // hit
    cache.get<int>("b");   // hit
    cache.get<int>("c");   // miss

    auto s = cache.stats();
    EXPECT_EQ(s.hits, 2u);
    EXPECT_EQ(s.misses, 1u);
    EXPECT_NEAR(s.hitRate, 2.0 / 3.0, 0.01);
    EXPECT_EQ(s.currentEntries, 2u);
}

// ── LRU eviction ───────────────────────────────────────────────────

TEST_F(QueryCacheTest, LruEvictionWhenAtCapacity)
{
    QueryCache smallCache(3);

    smallCache.put("a", 1);
    smallCache.put("b", 2);
    smallCache.put("c", 3);

    // Access "a" to make it most recently used.
    smallCache.get<int>("a");

    // Insert new entry — "b" should be evicted (oldest).
    smallCache.put("d", 4);

    EXPECT_TRUE(smallCache.get<int>("a").has_value());  // MRU
    EXPECT_FALSE(smallCache.get<int>("b").has_value()); // evicted
    EXPECT_TRUE(smallCache.get<int>("c").has_value());
    EXPECT_TRUE(smallCache.get<int>("d").has_value());  // newest
}

// ── Invalidation ────────────────────────────────────────────────────

TEST_F(QueryCacheTest, InvalidateByPrefix)
{
    cache.put("framestats/data1/0/10/50", std::vector<double>{1.0, 2.0});
    cache.put("framestats/data1/10/20/50", std::vector<double>{3.0, 4.0});
    cache.put("global/data1", 42);

    cache.invalidateByPrefix("framestats/");

    EXPECT_FALSE(cache.get<std::vector<double>>("framestats/data1/0/10/50").has_value());
    EXPECT_FALSE(cache.get<std::vector<double>>("framestats/data1/10/20/50").has_value());
    EXPECT_TRUE(cache.get<int>("global/data1").has_value());  // Not in prefix.
}

TEST_F(QueryCacheTest, InvalidateAll)
{
    cache.put("a", 1);
    cache.put("b", 2);
    cache.put("c", 3);

    cache.invalidateAll();

    EXPECT_EQ(cache.size(), 0u);
    EXPECT_FALSE(cache.get<int>("a").has_value());
}

// ── Complex types ──────────────────────────────────────────────────

struct FrameData {
    double zMin, zMax, rRms;
    int count;
};

// Required for nlohmann::json serialisation.
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(FrameData, zMin, zMax, rRms, count)

TEST_F(QueryCacheTest, StructSerializationRoundTrip)
{
    FrameData in{1.5, 2.5, 0.015, 1000};
    cache.put("frame/0", in);

    auto result = cache.get<FrameData>("frame/0");
    ASSERT_TRUE(result.has_value());
    EXPECT_NEAR(result->zMin, 1.5, 1e-9);
    EXPECT_NEAR(result->rRms, 0.015, 1e-9);
    EXPECT_EQ(result->count, 1000);
}

// ── Thread safety ──────────────────────────────────────────────────

TEST_F(QueryCacheTest, ConcurrentPutAndGet)
{
    std::atomic<bool> start{false};
    std::vector<std::thread> threads;
    QueryCache sharedCache(10000);

    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&, tid = t]() {
            while (!start.load()) std::this_thread::yield();
            for (int i = 0; i < 1000; ++i) {
                std::string key = "t" + std::to_string(tid) + "_" + std::to_string(i);
                sharedCache.put(key, i);
                auto val = sharedCache.get<int>(key);
                // May be nullopt if evicted, but must never crash.
                (void)val;  // may be nullopt if evicted, but must never crash
            }
        });
    }

    start.store(true);
    for (auto& th : threads) th.join();

    // No crash = pass.
    EXPECT_GE(sharedCache.stats().hits + sharedCache.stats().misses, 1000u);
}

TEST_F(QueryCacheTest, ConcurrentInvalidateByPrefix)
{
    QueryCache sharedCache;
    sharedCache.put("framestats/x/0/10/50", 1);
    sharedCache.put("framestats/y/10/20/50", 2);

    std::thread t1([&] { sharedCache.invalidateByPrefix("framestats/x"); });
    std::thread t2([&] { sharedCache.invalidateByPrefix("framestats/x"); });

    t1.join();
    t2.join();

    EXPECT_FALSE(sharedCache.get<int>("framestats/x/0/10/50").has_value());
}
