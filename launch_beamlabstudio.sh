#!/bin/bash
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"
if [ ! -f "${BUILD_DIR}/beamlab_ui" ]; then
    echo "Error: beamlab_ui not found in ${BUILD_DIR}. Build first with: cmake --build build"
    exit 1
fi
cd "${BUILD_DIR}"
exec ./beamlab_ui "$@"
