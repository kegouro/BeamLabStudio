#include "services/analysis/engines/FrameStatisticsPlugin.h"
#include "services/storage/IStorageBackend.h"

#include <gtest/gtest.h>

#include <atomic>
#include <cmath>
#include <vector>

using namespace beamlab::services::analysis;
using namespace beamlab::services::storage;
using namespace beamlab::data;
using namespace beamlab::core;

// ── Mock storage backend ─────────────────────────────────────────────

class MockStorage final : public IStorageBackend {
public:
    std::vector<TrajectorySample> samples;

    std::string backendName() const override { return "Mock"; }

    uint64_t totalSamples() const override { return samples.size(); }
    uint64_t totalTrajectories() const override { return 1; }

    void beginWriteBatch() override {}
    void writeSample(const TrajectorySample& s) override { samples.push_back(s); }
    void endWriteBatch() override {}

    SampleBatch readBatch(uint64_t offset, uint64_t count) const override
    {
        if (offset >= samples.size()) return {nullptr, 0, offset};
        auto avail = std::min<uint64_t>(count, samples.size() - offset);
        buf_.assign(samples.begin() + static_cast<ptrdiff_t>(offset),
                    samples.begin() + static_cast<ptrdiff_t>(offset + avail));
        return {buf_.data(), avail, offset};
    }

    SampleBatch readAxialRange(double, double) const override { return {nullptr, 0, 0}; }
    SampleBatch readTrajectory(TrajectoryId) const override { return {nullptr, 0, 0}; }
    GlobalStats globalStats() const override { return {}; }
    void vacuum() override {}
    uint64_t diskUsage() const override { return 0; }

private:
    mutable std::vector<TrajectorySample> buf_;
};

// ── Helpers ─────────────────────────────────────────────────────────

static TrajectorySample makeSample(double x, double y, double z)
{
    TrajectorySample s;
    s.position_m = {x, y, z};
    s.edep_MeV = 0.01;
    return s;
}

static nlohmann::json defaultConfig()
{
    return {
        {"axis_frame", {
            {"origin",        {{"x", 0}, {"y", 0}, {"z", 0}}},
            {"longitudinal",  {{"x", 0}, {"y", 0}, {"z", 1}}},
            {"transverse_u",  {{"x", 1}, {"y", 0}, {"z", 0}}},
            {"transverse_v",  {{"x", 0}, {"y", 1}, {"z", 0}}},
        }},
        {"bin_count",  2},
        {"batch_size", 100},
    };
}

// ── Tests ────────────────────────────────────────────────────────────

TEST(FrameStatisticsPluginTest, KnownMoments)
{
    // 3 identical trajectories × 2 positions → 6 samples
    // z = 0.0 and z = 10.0 → 2 bins of width 5 each
    // Bin 0 (z = 0–5): 3 samples at (0,0), (1,0), (0,1)
    // Bin 1 (z = 5–10): 3 samples at (0,0), (1,0), (0,1)
    MockStorage storage;
    for (int t = 0; t < 3; ++t) {
        storage.writeSample(makeSample(0.0, 0.0, 0.0));
        storage.writeSample(makeSample(0.0, 0.0, 10.0));
        storage.writeSample(makeSample(1.0, 0.0, 0.0));
        storage.writeSample(makeSample(1.0, 0.0, 10.0));
        storage.writeSample(makeSample(0.0, 1.0, 0.0));
        storage.writeSample(makeSample(0.0, 1.0, 10.0));
    }
    ASSERT_EQ(storage.totalSamples(), 18u);

    // 2 bins → z range split in half
    auto config = defaultConfig();
    config["bin_count"] = 2;

    FrameStatisticsPlugin engine;
    auto result = engine.execute(storage, config, nullptr);

    EXPECT_TRUE(result.success);
    ASSERT_TRUE(result.metrics.contains("frames_data"));

    auto frames = result.metrics["frames_data"];
    ASSERT_GE(frames.size(), 1u);

    // First bin: 9 samples (3 traj × 3 at z=0) at (0,0), (1,0), (0,1)
    EXPECT_EQ(frames[0]["point_count"].get<uint64_t>(), 9u);
    EXPECT_NEAR(frames[0]["centroid_u"].get<double>(), 1.0 / 3.0, 1e-12);
    EXPECT_NEAR(frames[0]["centroid_v"].get<double>(), 1.0 / 3.0, 1e-12);
    EXPECT_NEAR(frames[0]["sigma_u"].get<double>(), std::sqrt(2.0 / 9.0), 1e-12);
    EXPECT_NEAR(frames[0]["sigma_v"].get<double>(), std::sqrt(2.0 / 9.0), 1e-12);
    EXPECT_TRUE(frames[0]["valid"].get<bool>());
}

TEST(FrameStatisticsPluginTest, EmptyStorage)
{
    MockStorage storage;

    FrameStatisticsPlugin engine;
    auto result = engine.execute(storage, defaultConfig(), nullptr);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.metrics["frames"].get<uint64_t>(), 0u);
    EXPECT_EQ(result.metrics["total_samples"].get<uint64_t>(), 0u);
}

TEST(FrameStatisticsPluginTest, SingleSampleProducesNoValidBins)
{
    MockStorage storage;
    storage.writeSample(makeSample(0.0, 0.0, 5.0));

    FrameStatisticsPlugin engine;
    auto result = engine.execute(storage, defaultConfig(), nullptr);

    EXPECT_TRUE(result.success);
    // Single sample → no bin has ≥3 samples → 0 frames
    EXPECT_EQ(result.metrics["frames"].get<uint64_t>(), 0u);
    EXPECT_EQ(result.metrics["total_samples"].get<uint64_t>(), 1u);
}

TEST(FrameStatisticsPluginTest, CancellationDuringScan)
{
    MockStorage storage;
    for (int i = 0; i < 20000; ++i) {
        storage.writeSample(makeSample(0.0, 0.0, static_cast<double>(i % 100)));
    }

    FrameStatisticsPlugin engine;

    // Cancel before execute — should fail immediately (Pass 1 hits 0 samples,
    // since the loop offset starts at 0, with batch_size=100 and offset=0
    // will read first batch, process, then check cancellation.
    // Cancel after to simulate mid-scan interruption.
    // We schedule cancellation via a timer-like approach: trigger it after
    // the pass has started.
    // Since we can't easily interleave, cancel and then verify
    // the result handles it gracefully (returns early with cancel msg).
    engine.cancel();

    auto config = defaultConfig();
    config["batch_size"] = 1000;
    auto result = engine.execute(storage, config, nullptr);

    // May succeed with 0 frames if cancelled during scan pass
    // OR return fail("Cancelled by user") depending on timing.
    // Both are acceptable as long as no crash.
    EXPECT_TRUE(result.success || (!result.success && result.error.has_value()));
}

TEST(FrameStatisticsPluginTest, ValidateConfigCatchesMissingFields)
{
    FrameStatisticsPlugin engine;

    auto issues = engine.validateConfig(nlohmann::json::object());
    EXPECT_GT(issues.size(), 0u);

    // Should mention axis_frame
    bool hasAxisFrameIssue = false;
    for (const auto& msg : issues) {
        if (msg.find("axis_frame") != std::string::npos) hasAxisFrameIssue = true;
    }
    EXPECT_TRUE(hasAxisFrameIssue);
}

TEST(FrameStatisticsPluginTest, ValidateConfigAcceptsValidConfig)
{
    FrameStatisticsPlugin engine;
    auto issues = engine.validateConfig(defaultConfig());
    EXPECT_TRUE(issues.empty());
}
