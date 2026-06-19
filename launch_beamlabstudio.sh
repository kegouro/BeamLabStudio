#!/bin/bash
set -e

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${PROJECT_DIR}/build"
EXECUTABLE="${BUILD_DIR}/src/ui/beamlab_ui"

echo "═══════════════════════════════════════════"
echo "  BeamLabStudio Launcher"
echo "═══════════════════════════════════════════"
echo "  Project: ${PROJECT_DIR}"
echo "  Build:   ${BUILD_DIR}"
echo "  Binary:  ${EXECUTABLE}"
echo ""

# ── Check if rebuild is needed ─────────────────────────────────
NEEDS_REBUILD=false

if [ ! -f "${EXECUTABLE}" ]; then
    echo "[launch] No binary found — building..."
    NEEDS_REBUILD=true
else
    BIN_TIME=$(stat -c %Y "${EXECUTABLE}" 2>/dev/null || stat -f %m "${EXECUTABLE}")
    # Find the newest source file.
    SRC_TIME=$(find "${PROJECT_DIR}/src" \
        -type f \( -name "*.cpp" -o -name "*.h" -o -name "CMakeLists.txt" \) \
        -exec stat -c %Y {} + 2>/dev/null | sort -rn | head -1)

    if [ -n "${SRC_TIME}" ] && [ "${SRC_TIME}" -gt "${BIN_TIME}" ]; then
        echo "[launch] Source is newer than binary — rebuilding..."
        NEEDS_REBUILD=true
    else
        echo "[launch] Binary is up to date."
    fi
fi

# ── Configure if this is a fresh build ─────────────────────────
if [ ! -f "${BUILD_DIR}/CMakeCache.txt" ]; then
    echo "[launch] First build — configuring..."
    cmake -B "${BUILD_DIR}" -S "${PROJECT_DIR}" \
        -DCMAKE_BUILD_TYPE=Release \
        -DBEAMLAB_ENABLE_QT_UI=ON
fi

# ── Rebuild ───────────────────────────────────────────────────
if [ "${NEEDS_REBUILD}" = true ]; then
    cmake --build "${BUILD_DIR}" -j$(nproc)
fi

echo "[launch] Starting BeamLabStudio..."
echo ""

exec "${EXECUTABLE}" "$@"
