#include "analysis/focus/FocusDetector.h"
#include "analysis/focus/FocusCurveBuilder.h"
#include "analysis/statistics/FrameStatistics.h"
#include "analysis/statistics/FrameStatisticsEngine.h"
#include "analysis/envelope/ConvexHullEnvelopeExtractor.h"
#include "analysis/envelope/EnvelopeParameters.h"
#include "analysis/slice/SliceExtractor.h"
#include "analysis/surfaces/SurfaceBuilder.h"
#include "analysis/surfaces/ContourResampler.h"
#include "core/math/Vec2.h"
#include "core/math/Vec3.h"
#include "data/model/AxisFrame.h"
#include "data/model/BeamEnvelope.h"
#include "data/model/TrajectoryDataset.h"
#include "data/model/TrajectorySample.h"
#include "io/normalization/AxisFrameResolver.h"

#include <gtest/gtest.h>

#include <cmath>
#include <random>
#include <numbers>

namespace {

beamlab::data::TrajectoryDataset makeTestBeam(int nTraj, int nSteps, double focusZ, double waistR) {
    beamlab::data::TrajectoryDataset ds;
    ds.axis_frame.origin = {0, 0, 0};
    ds.axis_frame.longitudinal = {0, 0, 1};
    ds.axis_frame.transverse_u = {1, 0, 0};
    ds.axis_frame.transverse_v = {0, 1, 0};

    std::mt19937 rng(42);
    std::normal_distribution<double> rdist(waistR, waistR * 0.2);

    for (int t = 0; t < nTraj; ++t) {
        beamlab::data::Trajectory traj;
        double entryR = waistR * 3.0;
        double theta = (t * 2.0 * std::numbers::pi) / nTraj;
        double entryX = entryR * std::cos(theta);
        double entryY = entryR * std::sin(theta);
        double focusX = waistR * std::cos(theta) * (0.8 + rdist(rng) / waistR * 0.4);
        double focusY = waistR * std::sin(theta) * (0.8 + rdist(rng) / waistR * 0.4);

        for (int s = 0; s < nSteps; ++s) {
            double frac = static_cast<double>(s) / static_cast<double>(nSteps - 1);
            double z = 0 + (focusZ * 2.0) * frac;
            double x = entryX + (focusX - entryX) * frac;
            double y = entryY + (focusY - entryY) * frac;

            beamlab::data::TrajectorySample sample;
            sample.position_m = {x, y, z};
            sample.edep_MeV = 0.1;
            traj.samples.push_back(sample);
        }
        ds.trajectories.push_back(std::move(traj));
    }
    return ds;
}

} // namespace

// ── Test 1: ConvexHullEnvelopeExtractor — area formula validation ─

TEST(PhysicsRegression, EnvelopeAreaFormulaCorrect) {
    beamlab::analysis::ConvexHullEnvelopeExtractor extractor;

    // Build a 10mm radius circle (100 points)
    beamlab::data::BeamSlice slice;
    slice.slice_index = 0;
    slice.axial_position_m = 0.0;
    for (int i = 0; i < 100; ++i) {
        double angle = 2.0 * std::numbers::pi * i / 100.0;
        double r = 0.01; // 10 mm
        beamlab::data::BeamSlicePoint p;
        p.projected_uv_m = {r * std::cos(angle), r * std::sin(angle)};
        slice.points.push_back(p);
    }

    beamlab::analysis::EnvelopeParameters params;
    params.minimum_points = 3;
    auto env = extractor.extract(slice, params);

    ASSERT_TRUE(env.valid);
    EXPECT_NEAR(env.area_m2, std::numbers::pi * 0.01 * 0.01, 1e-6);
    double r_eq = std::sqrt(env.area_m2 / std::numbers::pi);
    EXPECT_NEAR(r_eq, 0.01, 1e-4);
}

// ── Test 2: ContourResampler — preserves perimeter ─

TEST(PhysicsRegression, ContourResamplerPreservesShape) {
    beamlab::analysis::ContourResampler resampler;
    std::vector<beamlab::core::Vec2> circle;
    for (int i = 0; i < 50; ++i) {
        double angle = 2.0 * std::numbers::pi * i / 50.0;
        circle.push_back({0.01 * std::cos(angle), 0.01 * std::sin(angle)});
    }

    auto resampled = resampler.resampleClosedContour(circle, 96);
    ASSERT_EQ(resampled.size(), 96u);

    for (const auto& p : resampled) {
        double r = std::sqrt(p.x * p.x + p.y * p.y);
        EXPECT_NEAR(r, 0.01, 0.002);
    }
}

// ── Test 3: Focusing beam — focus position detected correctly ─

TEST(PhysicsRegression, FocusDetectorOnConvergingBeam) {
    auto ds = makeTestBeam(100, 100, 0.5, 0.001);

    beamlab::analysis::FrameStatisticsParameters statsParams;
    statsParams.reference_mode = beamlab::analysis::ReferenceMode::AxialBins;
    statsParams.axial_bin_count = 100;

    beamlab::analysis::FrameStatisticsEngine statsEngine;
    auto stats = statsEngine.compute(ds, statsParams);
    ASSERT_GT(stats.size(), 10u);

    beamlab::analysis::FocusParameters focusParams;
    focusParams.metric = beamlab::analysis::FocusMetricType::TransverseRmsRadius;

    beamlab::analysis::FocusCurveBuilder builder;
    auto curve = builder.build(stats, focusParams);
    ASSERT_EQ(curve.points.size(), stats.size());

    beamlab::analysis::FocusDetector detector;
    auto focus = detector.detect(curve, focusParams);

    ASSERT_TRUE(focus.valid);
    EXPECT_GT(focus.focus_index, 0u);
    EXPECT_LT(focus.focus_index, curve.points.size());
    EXPECT_GT(focus.focus_metric_value, 0.0);
}

// ── Test 4: Envelope area is positive ─

TEST(PhysicsRegression, EnvelopeAreaPositive) {
    auto ds = makeTestBeam(50, 50, 0.5, 0.002);

    beamlab::analysis::FrameStatisticsEngine statsEngine;
    beamlab::analysis::FrameStatisticsParameters statsParams;
    statsParams.axial_bin_count = 50;
    auto stats = statsEngine.compute(ds, statsParams);

    beamlab::analysis::FocusCurveBuilder builder;
    beamlab::analysis::FocusParameters fparams;
    auto curve = builder.build(stats, fparams);
    beamlab::analysis::FocusDetector detector;
    auto focus = detector.detect(curve, fparams);
    ASSERT_TRUE(focus.valid);

    beamlab::analysis::SliceExtractor extractor;
    beamlab::analysis::ConvexHullEnvelopeExtractor envExtractor;
    beamlab::analysis::EnvelopeParameters envParams;
    envParams.minimum_points = 3;

    for (std::size_t i = focus.focus_index > 2 ? focus.focus_index - 2 : 0;
         i <= focus.focus_index + 2 && i < curve.points.size(); ++i) {
        auto slice = extractor.extractFrameSlice(ds, curve, i);
        if (slice.points.size() < 3) continue;
        auto env = envExtractor.extract(slice, envParams);
        if (env.valid) {
            EXPECT_GT(env.area_m2, 0.0);
            EXPECT_GT(env.boundary_points.size(), 2u);
        }
    }
}

// ── Test 5: SurfaceBuilder — generates valid OBJ-like mesh ─

TEST(PhysicsRegression, SurfaceBuilderProducesValidMesh) {
    auto ds = makeTestBeam(50, 50, 0.5, 0.002);

    beamlab::analysis::FrameStatisticsEngine statsEngine;
    beamlab::analysis::FrameStatisticsParameters statsParams;
    statsParams.axial_bin_count = 50;
    auto stats = statsEngine.compute(ds, statsParams);

    beamlab::analysis::FocusCurveBuilder fcb;
    beamlab::analysis::FocusParameters fparams;
    auto curve = fcb.build(stats, fparams);
    beamlab::analysis::FocusDetector fd;
    auto focus = fd.detect(curve, fparams);

    beamlab::analysis::SliceExtractor se;
    beamlab::analysis::ConvexHullEnvelopeExtractor cee;
    beamlab::analysis::EnvelopeParameters envParams;
    envParams.minimum_points = 3;

    std::vector<beamlab::data::BeamEnvelope> envelopes;
    for (std::size_t i = 0; i < curve.points.size(); ++i) {
        auto slice = se.extractFrameSlice(ds, curve, i);
        auto env = cee.extract(slice, envParams);
        if (env.valid) envelopes.push_back(env);
    }
    ASSERT_GT(envelopes.size(), 5u);

    beamlab::analysis::SurfaceBuilder sb;
    beamlab::analysis::SurfaceBuildParameters sbParams;
    sbParams.resample_point_count = 48;
    auto surface = sb.build(envelopes, ds.axis_frame, sbParams);

    ASSERT_TRUE(surface.valid);
    EXPECT_GT(surface.mesh.vertices.size(), 100u);
    EXPECT_GT(surface.mesh.faces.size(), 100u);

    for (const auto& face : surface.mesh.faces) {
        EXPECT_LT(face.a, surface.mesh.vertices.size());
        EXPECT_LT(face.b, surface.mesh.vertices.size());
        EXPECT_LT(face.c, surface.mesh.vertices.size());
    }

    double zmin = 1e20, zmax = -1e20;
    for (const auto& v : surface.mesh.vertices) {
        zmin = std::min(zmin, v.position.z);
        zmax = std::max(zmax, v.position.z);
    }
    EXPECT_GT(zmax - zmin, 0.0);
}

// ── Test 6: AxisFrameResolver — z detected as longest ─

TEST(PhysicsRegression, AxisFrameResolverDetectsZ) {
    // Use a realistic beam: random z-perturbation per step so displacement
    // vectors do not all share the same exact delta-z (as real Geant4 data).
    beamlab::data::TrajectoryDataset ds;
    ds.axis_frame.origin = {0, 0, 0};
    ds.axis_frame.longitudinal = {0, 0, 1};
    ds.axis_frame.transverse_u = {1, 0, 0};
    ds.axis_frame.transverse_v = {0, 1, 0};

    std::mt19937 rng(42);
    std::normal_distribution<double> posNoise(0.0, 0.0001);   // 0.1 mm positional jitter
    std::normal_distribution<double> stepLen(0.02, 0.005);    // variable step length ~2 cm

    const int nTraj = 20;
    const int nSteps = 40;
    const double zStart = 11.55;

    for (int t = 0; t < nTraj; ++t) {
        beamlab::data::Trajectory traj;
        double z = zStart;
        for (int s = 0; s < nSteps; ++s) {
            beamlab::data::TrajectorySample sample;
            sample.position_m.x = posNoise(rng);
            sample.position_m.y = posNoise(rng);
            sample.position_m.z = z;
            sample.edep_MeV = 0.1;
            traj.samples.push_back(sample);
            z += std::abs(stepLen(rng));
        }
        ds.trajectories.push_back(std::move(traj));
    }

    beamlab::io::AxisFrameResolver resolver;
    auto frame = resolver.resolve(ds);

    EXPECT_EQ(frame.derivation_method, "pca_on_displacement_vectors");
    EXPECT_GT(dot(frame.longitudinal, beamlab::core::Vec3{0.0, 0.0, 1.0}), 0.99);
    EXPECT_GT(frame.confidence, 0.9);
}
