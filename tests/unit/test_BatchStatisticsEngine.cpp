#include "analysis/statistics/BatchStatisticsEngine.h"
#include "analysis/statistics/FrameStatisticsEngine.h"
#include "core/storage/InMemoryStorage.h"
#include "data/model/TrajectoryDataset.h"
#include "data/model/Trajectory.h"
#include "data/model/TrajectorySample.h"

#include <gtest/gtest.h>

#include <cmath>
#include <vector>

using namespace beamlab::analysis;
using namespace beamlab::data;
using namespace beamlab::core;

static TrajectorySample makeS(double x, double y, double z, double edep, double kine)
{
    TrajectorySample s{};
    s.position_m = {x, y, z};
    s.edep_MeV = edep;
    s.kinE_MeV = kine;
    return s;
}

TEST(BatchStatisticsEngineTest, MatchesOriginalEngine)
{
    AxisFrame ax = {{0,0,0}, {0,0,1}, {1,0,0}, {0,1,0}};

    // Build original TrajectoryDataset
    TrajectoryDataset ds;
    ds.axis_frame = ax;
    ds.variables.registerVariable({"track_id", "trackID", "1", false});
    ds.variables.registerVariable({"event_id", "eventID", "1", false});
    Trajectory traj1;
    traj1.samples.push_back(makeS(0.0, 0.0, 0.0, 0.01, 100.0));
    traj1.samples.push_back(makeS(0.0, 0.0, 5.0, 0.02, 99.0));
    traj1.samples.push_back(makeS(0.0, 0.0, 10.0, 0.03, 98.0));
    ds.trajectories.push_back(traj1);

    Trajectory traj2;
    traj2.samples.push_back(makeS(1.0, 0.0, 0.0, 0.01, 50.0));
    traj2.samples.push_back(makeS(1.0, 0.0, 5.0, 0.02, 49.0));
    traj2.samples.push_back(makeS(1.0, 0.0, 10.0, 0.03, 48.0));
    ds.trajectories.push_back(traj2);

    Trajectory traj3;
    traj3.samples.push_back(makeS(0.0, 1.0, 0.0, 0.01, 30.0));
    traj3.samples.push_back(makeS(0.0, 1.0, 5.0, 0.02, 29.0));
    traj3.samples.push_back(makeS(0.0, 1.0, 10.0, 0.03, 28.0));
    ds.trajectories.push_back(traj3);

    // Build storage
    InMemoryStorage storage;
    storage.beginTrajectory("1");
    for (const auto& s : traj1.samples) storage.addSample(s);
    storage.endTrajectory();
    storage.beginTrajectory("2");
    for (const auto& s : traj2.samples) storage.addSample(s);
    storage.endTrajectory();
    storage.beginTrajectory("3");
    for (const auto& s : traj3.samples) storage.addSample(s);
    storage.endTrajectory();

    // Original engine
    FrameStatisticsEngine origEngine;
    FrameStatisticsParameters params;
    params.reference_mode = ReferenceMode::AxialBins;
    params.axial_binning_mode = AxialBinningMode::Uniform;
    params.axial_bin_count = 2;
    auto orig = origEngine.compute(ds, params);

    // Batch engine
    BatchStatisticsEngine batchEngine;
    auto batch = batchEngine.compute(storage, ax, params);

    ASSERT_EQ(orig.size(), batch.size());
    for (size_t i = 0; i < orig.size(); ++i) {
        EXPECT_NEAR(orig[i].centroid_u, batch[i].centroid_u, 1e-12);
        EXPECT_NEAR(orig[i].centroid_v, batch[i].centroid_v, 1e-12);
        EXPECT_NEAR(orig[i].sigma_u, batch[i].sigma_u, 1e-12);
        EXPECT_NEAR(orig[i].sigma_v, batch[i].sigma_v, 1e-12);
        EXPECT_NEAR(orig[i].r_rms, batch[i].r_rms, 1e-12);
    }
}

TEST(BatchStatisticsEngineTest, EmptyStorage)
{
    InMemoryStorage storage;
    AxisFrame ax = {{0,0,0}, {0,0,1}, {1,0,0}, {0,1,0}};
    BatchStatisticsEngine engine;
    auto result = engine.compute(storage, ax);
    EXPECT_TRUE(result.empty());
}

TEST(BatchStatisticsEngineTest, SingleTrajectoryThreeSamples)
{
    InMemoryStorage storage;
    storage.beginTrajectory("1");
    storage.addSample(makeS(0.0, 0.0, 0.0, 0.1, 100.0));
    storage.addSample(makeS(0.0, 0.0, 5.0, 0.2, 99.0));
    storage.addSample(makeS(0.0, 0.0, 10.0, 0.3, 98.0));
    storage.endTrajectory();

    AxisFrame ax = {{0,0,0}, {0,0,1}, {1,0,0}, {0,1,0}};
    FrameStatisticsParameters params;
    params.reference_mode = ReferenceMode::AxialBins;
    params.axial_binning_mode = AxialBinningMode::Uniform;
    params.axial_bin_count = 1;  // < 2 => uses auto bin count

    BatchStatisticsEngine engine;
    auto result = engine.compute(storage, ax, params);
    // With 3 samples and auto bin count, we get at least 1 bin if count >= 3
    // Actually auto bin count = total/500 = 3/500 = 0 → clamped to 24 bins
    // With 3 samples in 24 bins, no bin has >= 3 → empty result
    EXPECT_TRUE(result.empty());
}
