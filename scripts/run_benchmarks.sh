#!/usr/bin/env bash
# ══════════════════════════════════════════════════════════════════════
#  run_benchmarks.sh — Run BeamLabStudio performance benchmarks.
#  Usage: bash scripts/run_benchmarks.sh [--json] [--filter FILTER]
# ══════════════════════════════════════════════════════════════════════

set -euo pipefail
cd "$(git rev-parse --show-toplevel 2>/dev/null || pwd)"

BUILD_DIR="${1:-build-release}"
JSON_OUTPUT="${2:-}"

# ── Build (release mode for accurate timing) ─────────────────────
echo "══════════════════════════════════════════════════════════════"
echo "  BeamLabStudio — Performance Benchmarks"
echo "══════════════════════════════════════════════════════════════"
echo ""

if [ ! -d "$BUILD_DIR" ]; then
    echo "▸ Configuring Release build in $BUILD_DIR..."
    cmake -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release -DBEAMLAB_ENABLE_QT_UI=OFF
fi

echo "▸ Building performance tests..."
cmake --build "$BUILD_DIR" --target beamlab_performance_tests -j"$(nproc)" 2>&1 | tail -3

echo ""
echo "▸ Running benchmarks (ctest -L performance)..."
echo ""

if [ -n "$JSON_OUTPUT" ]; then
    ctest --test-dir "$BUILD_DIR" -L performance --verbose \
          --output-junit "$JSON_OUTPUT" 2>&1
    echo ""
    echo "JSON report written to: $JSON_OUTPUT"
else
    ctest --test-dir "$BUILD_DIR" -L performance --verbose 2>&1
fi

echo ""
echo "══════════════════════════════════════════════════════════════"
echo "  Benchmarks complete."
echo "  Filter:  ctest --test-dir $BUILD_DIR -L performance --verbose"
echo "  JSON:    ctest --test-dir $BUILD_DIR -L performance --output-junit report.xml"
echo "══════════════════════════════════════════════════════════════"
