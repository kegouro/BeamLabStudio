#include "data/repositories/SqliteTrajectoryRepository.h"
#include "services/storage/SqliteBackend.h"
#include "services/storage/QueryCache.h"
#include "core/config/AnalysisConfig.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <memory>

namespace fs = std::filesystem;
using namespace beamlab::data;
using namespace beamlab::data::repositories;
using namespace beamlab::services::storage;
using namespace beamlab::core;

class SqliteRepoCacheTest : public ::testing::Test {
protected:
    std::shared_ptr<SqliteBackend> backend;
    std::shared_ptr<QueryCache> cache;
    std::unique_ptr<SqliteTrajectoryRepository> repo;

    void SetUp() override
    {
        auto dbPath = (fs::temp_directory_path() / "test_repo_cache_XXXXXX.db").string();
        // Replace X with unique id.
        auto now = std::chrono::steady_clock::now().time_since_epoch().count();
        auto pos = dbPath.find("XXXXXX");
        if (pos != std::string::npos)
            dbPath.replace(pos, 6, std::to_string(now));

        backend = std::make_shared<SqliteBackend>(dbPath);
        cache   = std::make_shared<QueryCache>(100);
        repo    = std::make_unique<SqliteTrajectoryRepository>(backend, cache);

        // Seed with a few samples.
        backend->beginWriteBatch();
        for (int i = 0; i < 50; ++i) {
            TrajectorySample s;
            s.trajectory_id = TrajectoryId(1);
            s.position_m = {static_cast<double>(i) * 0.01,
                            static_cast<double>(i) * 0.005,
                            static_cast<double>(i) * 0.02};
            s.edep_MeV = 0.01;
            backend->writeSample(s);
        }
        backend->endWriteBatch();
        backend->finalizeIndices();
    }
};

// ── Cache hit / miss ───────────────────────────────────────────────

TEST_F(SqliteRepoCacheTest, FirstCallIsMiss)
{
    AxisFrame axis;
    axis.longitudinal = {0, 0, 1};
    axis.transverse_u = {1, 0, 0};
    axis.transverse_v = {0, 1, 0};
    axis.derivation_method = "test";
    axis.confidence = 1.0;

    BinningConfig binning;
    binning.axial_bins = 10;

    auto s = cache->stats();
    auto beforeMisses = s.misses;

    auto frames = repo->computeFrameStats(axis, binning);
    EXPECT_GT(frames.size(), 0u);

    auto s2 = cache->stats();
    EXPECT_GT(s2.misses, beforeMisses);  // A miss was recorded.
}

TEST_F(SqliteRepoCacheTest, SecondCallIsHit)
{
    AxisFrame axis;
    axis.longitudinal = {0, 0, 1};
    axis.transverse_u = {1, 0, 0};
    axis.transverse_v = {0, 1, 0};
    axis.derivation_method = "test";
    axis.confidence = 1.0;

    BinningConfig binning;
    binning.axial_bins = 5;

    // First call — miss.
    repo->computeFrameStats(axis, binning);

    auto s = cache->stats();
    auto beforeHits = s.hits;

    // Second call with same parameters — hit.
    repo->computeFrameStats(axis, binning);

    auto s2 = cache->stats();
    EXPECT_GT(s2.hits, beforeHits);
}

// ── Invalidation ───────────────────────────────────────────────────

TEST_F(SqliteRepoCacheTest, InsertSampleInvalidatesCache)
{
    AxisFrame axis;
    axis.longitudinal = {0, 0, 1};
    axis.transverse_u = {1, 0, 0};
    axis.transverse_v = {0, 1, 0};
    BinningConfig binning;
    binning.axial_bins = 5;

    // Populate cache.
    repo->computeFrameStats(axis, binning);

    auto s1 = cache->stats();
    auto hitsBefore = s1.hits;

    // Insert a new sample — should invalidate.
    TrajectorySample newSample;
    newSample.trajectory_id = TrajectoryId(1);
    newSample.position_m = {0.05, 0.01, 0.5};
    newSample.edep_MeV = 0.02;
    repo->insertSample(newSample);

    // Next call should be a miss (cache invalidated).
    repo->computeFrameStats(axis, binning);

    auto s2 = cache->stats();
    EXPECT_GT(s2.misses, s1.misses) << "Expected cache miss after insert";
}

// ── Different parameters produce different keys ────────────────────

TEST_F(SqliteRepoCacheTest, DifferentBinningIsDifferentKey)
{
    AxisFrame axis;
    axis.longitudinal = {0, 0, 1};
    axis.transverse_u = {1, 0, 0};
    axis.transverse_v = {0, 1, 0};
    axis.derivation_method = "test";

    BinningConfig b1; b1.axial_bins = 10;
    BinningConfig b2; b2.axial_bins = 20;

    repo->computeFrameStats(axis, b1);

    auto s = cache->stats();
    auto missesBefore = s.misses;

    repo->computeFrameStats(axis, b2);  // Different bin count → miss.

    auto s2 = cache->stats();
    EXPECT_GT(s2.misses, missesBefore);
}

// ── Null cache — always miss ───────────────────────────────────────

TEST_F(SqliteRepoCacheTest, NullCacheAlwaysMisses)
{
    auto dbPath = (fs::temp_directory_path() / "test_nocache.db").string();
    auto noCacheBackend = std::make_shared<SqliteBackend>(dbPath);
    noCacheBackend->beginWriteBatch();
    TrajectorySample s;
    s.trajectory_id = TrajectoryId(1);
    s.position_m = {0.1, 0.2, 0.3};
    s.edep_MeV = 0.01;
    noCacheBackend->writeSample(s);
    noCacheBackend->endWriteBatch();

    SqliteTrajectoryRepository noCacheRepo(noCacheBackend, nullptr);

    AxisFrame axis;
    axis.longitudinal = {0, 0, 1};
    axis.transverse_u = {1, 0, 0};
    axis.transverse_v = {0, 1, 0};
    BinningConfig b; b.axial_bins = 2;

    // Should not crash even with null cache.
    auto frames = noCacheRepo.computeFrameStats(axis, b);
    EXPECT_GE(frames.size(), 0u);
}

// ── Frame statistics correctness ───────────────────────────────────

TEST_F(SqliteRepoCacheTest, FrameStatsHaveCorrectBins)
{
    AxisFrame axis;
    axis.longitudinal = {0, 0, 1};
    axis.transverse_u = {1, 0, 0};
    axis.transverse_v = {0, 1, 0};
    BinningConfig binning;
    binning.axial_bins = 5;

    auto frames = repo->computeFrameStats(axis, binning);

    // Should have between 0 and 5 frames.
    EXPECT_LE(frames.size(), 5u);

    for (const auto& f : frames) {
        EXPECT_TRUE(f.valid);
        EXPECT_GE(f.point_count, 3u) << "Each frame must have ≥3 samples";
    }
}
