#include "analysis/focus/FocusDetector.h"
#include "analysis/focus/FocusParameters.h"
#include "data/model/FocusCurve.h"
#include "data/model/FocusResult.h"

#include <gtest/gtest.h>

#include <vector>

using namespace beamlab::analysis;
using namespace beamlab::data;

FocusCurve makeParabola()
{
    // y = (x - 3)^2  → minimum at x = 3, y = 0
    FocusCurve curve;
    curve.metric_name = "test";
    for (int i = 0; i <= 6; ++i) {
        FocusCurvePoint p;
        p.reference_value = static_cast<double>(i);
        p.metric_value = static_cast<double>((i - 3) * (i - 3));
        p.point_count = 10;
        curve.points.push_back(p);
    }
    return curve;
}

TEST(FocusDetectorTest, KnownMinimum)
{
    FocusDetector detector;
    FocusParameters params;
    // Disable smoothing so we get the exact min
    params.smooth_curve = false;

    const auto curve = makeParabola();
    const auto result = detector.detect(curve, params);

    EXPECT_TRUE(result.valid);
    EXPECT_EQ(result.focus_index, 3u);
    EXPECT_DOUBLE_EQ(result.focus_reference_value, 3.0);
    EXPECT_DOUBLE_EQ(result.focus_metric_value, 0.0);
    EXPECT_GT(result.confidence, 0.0);
}

TEST(FocusDetectorTest, FlatCurveNoMinimum)
{
    // All y values equal → no clear minimum
    FocusCurve curve;
    curve.metric_name = "flat";
    for (int i = 0; i < 5; ++i) {
        FocusCurvePoint p;
        p.reference_value = static_cast<double>(i);
        p.metric_value = 1.0;
        p.point_count = 10;
        curve.points.push_back(p);
    }

    FocusDetector detector;
    FocusParameters params;
    params.smooth_curve = false;

    const auto result = detector.detect(curve, params);
    // A flat curve should still return valid with confidence ~0
    EXPECT_TRUE(result.valid);
    EXPECT_LE(result.confidence, 0.01);
}

TEST(FocusDetectorTest, EdgeMinimum)
{
    FocusCurve curve;
    curve.metric_name = "edge";
    // Monotonically decreasing → minimum at last point
    for (int i = 0; i < 4; ++i) {
        FocusCurvePoint p;
        p.reference_value = static_cast<double>(i);
        p.metric_value = static_cast<double>(10 - i);
        p.point_count = 10;
        curve.points.push_back(p);
    }

    FocusDetector detector;
    FocusParameters params;
    params.smooth_curve = false;

    const auto result = detector.detect(curve, params);
    EXPECT_TRUE(result.valid);
    EXPECT_EQ(result.focus_index, 3u);
}
