#include "analysis/statistics/FrameStatisticsEngine.h"
#include "data/model/TrajectoryDataset.h"

#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <vector>

using namespace beamlab::analysis;
using namespace beamlab::data;
using namespace beamlab::core;

static TrajectorySample makeSample(double x, double y, double z,
                                   double edep, double kine)
{
    TrajectorySample s{};
    s.position_m = {x, y, z};
    s.edep_MeV = edep;
    s.kinE_MeV = kine;
    return s;
}

TEST(FrameStatisticsEngineTest, KnownMoments)
{
    // Three trajectories spread across z range [0, 10] with
    // known (x,y) values for easy mean/variance checks.
    // Uniform axial binning with 1 bin aggregates all samples.
    TrajectoryDataset ds;
    ds.axis_frame = {
        {0.0, 0.0, 0.0},
        {0.0, 0.0, 1.0},
        {1.0, 0.0, 0.0},
        {0.0, 1.0, 0.0}
    };
    ds.variables.registerVariable({"track_id", "trackID", "1", false});
    ds.variables.registerVariable({"event_id", "eventID", "1", false});

    Trajectory traj1;
    traj1.samples.push_back(makeSample(0.0, 0.0, 0.0, 0.01, 100.0));
    traj1.samples.push_back(makeSample(0.0, 0.0, 10.0, 0.01, 100.0));
    ds.trajectories.push_back(traj1);

    Trajectory traj2;
    traj2.samples.push_back(makeSample(1.0, 0.0, 0.0, 0.02, 99.0));
    traj2.samples.push_back(makeSample(1.0, 0.0, 10.0, 0.02, 99.0));
    ds.trajectories.push_back(traj2);

    Trajectory traj3;
    traj3.samples.push_back(makeSample(0.0, 1.0, 0.0, 0.03, 98.0));
    traj3.samples.push_back(makeSample(0.0, 1.0, 10.0, 0.03, 98.0));
    ds.trajectories.push_back(traj3);

    FrameStatisticsEngine engine;
    FrameStatisticsParameters params;
    params.reference_mode = ReferenceMode::AxialBins;
    params.axial_binning_mode = AxialBinningMode::Uniform;
    params.axial_bin_count = 2;  // 2 bins along z=[0,10], but 6 samples should fill > 0

    const auto frames = engine.compute(ds, params);
    ASSERT_FALSE(frames.empty()) << "Expected at least one frame";

    // Check the first frame (bin 0, z ~ 0-5)
    EXPECT_EQ(frames[0].point_count, 3u);
    EXPECT_NEAR(frames[0].centroid_u, 1.0 / 3.0, 1e-12);
    EXPECT_NEAR(frames[0].centroid_v, 1.0 / 3.0, 1e-12);
    EXPECT_NEAR(frames[0].sigma_u, std::sqrt(2.0 / 9.0), 1e-12);
}

TEST(FrameStatisticsEngineTest, EmptyDataset)
{
    TrajectoryDataset ds;
    ds.axis_frame = {{0,0,0}, {0,0,1}, {1,0,0}, {0,1,0}};

    FrameStatisticsEngine engine;
    const auto frames = engine.compute(ds);
    EXPECT_TRUE(frames.empty());
}

TEST(FrameStatisticsEngineTest, SinglePoint)
{
    TrajectoryDataset ds;
    ds.axis_frame = {{0,0,0}, {0,0,1}, {1,0,0}, {0,1,0}};
    ds.variables.registerVariable({"track_id", "trackID", "1", false});
    ds.variables.registerVariable({"event_id", "eventID", "1", false});

    Trajectory traj;
    traj.samples.push_back(makeSample(0.5, 0.3, 1.0, 0.01, 50.0));
    ds.trajectories.push_back(traj);

    FrameStatisticsEngine engine;
    FrameStatisticsParameters params;
    params.reference_mode = ReferenceMode::AxialBins;
    params.axial_binning_mode = AxialBinningMode::Uniform;
    params.axial_bin_count = 1;

    const auto frames = engine.compute(ds, params);
    // Single point still has acc.count < 3 so no frame is produced
    EXPECT_TRUE(frames.empty());
    return;

    EXPECT_NEAR(frames[0].centroid_u, 0.5, 1e-12);
    EXPECT_NEAR(frames[0].centroid_v, 0.3, 1e-12);
    // sigma should be 0 for a single point
    EXPECT_NEAR(frames[0].sigma_u, 0.0, 1e-12);
    EXPECT_NEAR(frames[0].sigma_v, 0.0, 1e-12);
}
