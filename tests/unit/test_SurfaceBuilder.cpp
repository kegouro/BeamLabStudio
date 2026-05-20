#include "analysis/surfaces/SurfaceBuilder.h"
#include "analysis/surfaces/SurfaceBuildParameters.h"
#include "core/math/Vec2.h"
#include "data/model/AxisFrame.h"
#include "data/model/BeamEnvelope.h"
#include "data/model/LensSurfaceModel.h"

#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <vector>

using namespace beamlab::analysis;
using namespace beamlab::data;
using namespace beamlab::core;

// Build two circular envelopes at different axial positions
static std::vector<BeamEnvelope> makeTwoRings()
{
    std::vector<BeamEnvelope> envelopes;
    envelopes.resize(2);

    // Ring 0 at z = 0, radius 1.0
    envelopes[0].slice_index = 0;
    envelopes[0].axial_position_m = 0.0;
    envelopes[0].valid = true;
    for (int i = 0; i < 8; ++i) {
        const double angle = 2.0 * M_PI * static_cast<double>(i) / 8.0;
        envelopes[0].boundary_points.push_back({std::cos(angle), std::sin(angle)});
    }

    // Ring 1 at z = 1.0, radius 0.5
    envelopes[1].slice_index = 1;
    envelopes[1].axial_position_m = 1.0;
    envelopes[1].valid = true;
    for (int i = 0; i < 8; ++i) {
        const double angle = 2.0 * M_PI * static_cast<double>(i) / 8.0;
        envelopes[1].boundary_points.push_back({0.5 * std::cos(angle), 0.5 * std::sin(angle)});
    }

    return envelopes;
}

TEST(SurfaceBuilderTest, VerticesAndFaces)
{
    const auto envelopes = makeTwoRings();
    const AxisFrame axis_frame = {
        {0.0, 0.0, 0.0},           // origin
        {0.0, 0.0, 1.0},           // longitudinal = z
        {1.0, 0.0, 0.0},           // u = x
        {0.0, 1.0, 0.0}            // v = y
    };

    SurfaceBuildParameters params;
    params.resample_point_count = 8;  // keep the 8 points we have

    SurfaceBuilder builder;
    const auto result = builder.build(envelopes, axis_frame, params);

    EXPECT_TRUE(result.valid);
    EXPECT_EQ(result.slice_count, 2u);
    EXPECT_EQ(result.points_per_slice, 8u);

    // 2 rings × 8 points = 16 vertices
    EXPECT_EQ(result.mesh.vertices.size(), 16u);

    // 2 rings → 1 ring pair × 2 triangles per segment × 8 segments = 16 faces
    EXPECT_EQ(result.mesh.faces.size(), 16u);
}

TEST(SurfaceBuilderTest, SingleRingReturnsInvalid)
{
    std::vector<BeamEnvelope> envelopes;
    envelopes.resize(1);
    envelopes[0].slice_index = 0;
    envelopes[0].axial_position_m = 0.0;
    envelopes[0].valid = true;
    envelopes[0].boundary_points.push_back({1.0, 0.0});

    const AxisFrame axis_frame = {{0,0,0}, {0,0,1}, {1,0,0}, {0,1,0}};
    SurfaceBuildParameters params;

    SurfaceBuilder builder;
    const auto result = builder.build(envelopes, axis_frame, params);
    EXPECT_FALSE(result.valid);
}

TEST(SurfaceBuilderTest, EmptyEnvelopes)
{
    const AxisFrame axis_frame = {{0,0,0}, {0,0,1}, {1,0,0}, {0,1,0}};
    SurfaceBuildParameters params;

    SurfaceBuilder builder;
    const auto result = builder.build({}, axis_frame, params);
    EXPECT_FALSE(result.valid);
}
