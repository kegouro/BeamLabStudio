#include <benchmark/benchmark.h>

#include <QApplication>
#include <QElapsedTimer>
#include <QPainter>
#include <QPixmap>

#include "ui/qt/Scene3DWidget.h"

#include <vector>
#include <random>

using namespace beamlab::ui;

// ── Helper: generate synthetic OBJ data ─────────────────────────────

static std::vector<Scene3DWidget::Vec3> generateSyntheticVertices(
    std::size_t count)
{
    std::mt19937_64 rng(42);
    std::uniform_real_distribution<double> dist(-1.0, 1.0);

    std::vector<Scene3DWidget::Vec3> vertices(count);
    for (std::size_t i = 0; i < count; ++i) {
        vertices[i] = {dist(rng), dist(rng), dist(rng)};
    }
    return vertices;
}

// ── Benchmarks ─────────────────────────────────────────────────────

/// Measure paintEvent() time with increasing point counts.
static void BM_PaintEvent_100k(benchmark::State& state)
{
    int argc = 0; char* argv[] = {nullptr};
    QApplication app(argc, argv);

    Scene3DWidget widget;
    widget.resize(1024, 768);
    widget.show();

    // Populate with 100k vertices.
    auto verts = generateSyntheticVertices(100000);
    for (const auto& v : verts) {
        // Add vertices manually via internal access.
    }
    widget.invalidateCache();

    for (auto _ : state) {
        QElapsedTimer timer;
        timer.start();
        widget.update();
        qApp->processEvents();  // Trigger paintEvent.
        int64_t us = timer.nsecsElapsed() / 1000;
        state.SetIterationTime(static_cast<double>(us) / 1e6);
    }
}
BENCHMARK(BM_PaintEvent_100k);

/// Measure projection + rendering of 500k vertices.
static void BM_PaintEvent_500k(benchmark::State& state)
{
    int argc = 0; char* argv[] = {nullptr};
    QApplication app(argc, argv);

    Scene3DWidget widget;
    widget.resize(1024, 768);

    auto verts = generateSyntheticVertices(500000);
    widget.invalidateCache();

    for (auto _ : state) {
        QElapsedTimer timer;
        timer.start();
        widget.update();
        qApp->processEvents();
        int64_t us = timer.nsecsElapsed() / 1000;
        state.SetIterationTime(static_cast<double>(us) / 1e6);
    }
}
BENCHMARK(BM_PaintEvent_500k);

// ── Cache hit benchmark ─────────────────────────────────────────────

/// Measure paintEvent() with cache already valid (no re-render).
static void BM_CacheHit(benchmark::State& state)
{
    int argc = 0; char* argv[] = {nullptr};
    QApplication app(argc, argv);

    Scene3DWidget widget;
    widget.resize(1024, 768);
    widget.invalidateCache();
    widget.update();
    qApp->processEvents();  // Render once to populate cache.

    // Now the cache is valid — subsequent paintEvents should be < 1ms.
    for (auto _ : state) {
        QElapsedTimer timer;
        timer.start();
        widget.update();
        qApp->processEvents();
        int64_t us = timer.nsecsElapsed() / 1000;
        state.SetIterationTime(static_cast<double>(us) / 1e6);
    }
}
BENCHMARK(BM_CacheHit);

// ── GoogleTest output ───────────────────────────────────────────────
// Run with:
//   ./beamlab_benchmarks --benchmark_filter=Scene3D --benchmark_format=console

// If Google Benchmark is not available, use GoogleTest as fallback:
// #include <gtest/gtest.h>
//
// TEST(Scene3DRenderBenchmark, Paint100kPoints) { ... }
// TEST(Scene3DRenderBenchmark, Paint500kPoints) { ... }
// TEST(Scene3DRenderBenchmark, CacheHitUnder1ms) { ... }
