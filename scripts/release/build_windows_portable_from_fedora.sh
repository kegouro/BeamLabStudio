#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build-win64"
DIST_ROOT="$PROJECT_ROOT/dist"
DIST_DIR="$DIST_ROOT/BeamLabStudio-win64-portable"
ZIP_PATH="$DIST_ROOT/BeamLabStudio-win64-portable.zip"

echo "== BeamLabStudio Windows portable build from Fedora =="
echo "Project: $PROJECT_ROOT"

need_cmd() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "Missing command: $1"
        exit 1
    fi
}

need_cmd cmake
need_cmd ninja
need_cmd zip
need_cmd rsync
need_cmd x86_64-w64-mingw32-g++
need_cmd x86_64-w64-mingw32-objdump

TOOLCHAIN="$(find /usr/share /usr -name 'toolchain-mingw64.cmake' 2>/dev/null | head -n 1 || true)"
QT6_CONFIG="$(find /usr/x86_64-w64-mingw32 -path '*/lib/cmake/Qt6/Qt6Config.cmake' 2>/dev/null | head -n 1 || true)"

if [[ -z "$TOOLCHAIN" ]]; then
    echo "Could not find toolchain-mingw64.cmake"
    echo "Try: sudo dnf install mingw64-filesystem mingw64-gcc-c++"
    exit 1
fi

if [[ -z "$QT6_CONFIG" ]]; then
    echo "Could not find MinGW Qt6Config.cmake"
    echo "Try: sudo dnf install mingw64-qt6-qtbase mingw64-qt6-qtsvg"
    exit 1
fi

QT6_DIR="$(dirname "$QT6_CONFIG")"
MINGW_PREFIX="$(cd "$QT6_DIR/../../.." && pwd)"
MINGW_BIN="$MINGW_PREFIX/bin"

QWINDOWS_DLL="$(find "$MINGW_PREFIX" -path '*/plugins/platforms/qwindows.dll' 2>/dev/null | head -n 1 || true)"
QT_PLUGIN_ROOT=""
if [[ -n "$QWINDOWS_DLL" ]]; then
    QT_PLUGIN_ROOT="$(cd "$(dirname "$QWINDOWS_DLL")/.." && pwd)"
fi

echo "Toolchain:     $TOOLCHAIN"
echo "Qt6_DIR:       $QT6_DIR"
echo "MinGW prefix:  $MINGW_PREFIX"
echo "Qt plugins:    ${QT_PLUGIN_ROOT:-not found}"

rm -rf "$BUILD_DIR" "$DIST_DIR" "$ZIP_PATH"
mkdir -p "$BUILD_DIR" "$DIST_DIR"

echo
echo "== Configuring =="
cmake \
    -S "$PROJECT_ROOT" \
    -B "$BUILD_DIR" \
    -G Ninja \
    -DCMAKE_MAKE_PROGRAM="$(command -v ninja)" \
    -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
    -DCMAKE_PREFIX_PATH="$MINGW_PREFIX" \
    -DQt6_DIR="$QT6_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBEAMLAB_ENABLE_QT_UI=ON \
    -DBEAMLAB_ENABLE_ROOT=OFF

echo
echo "== Building =="
cmake --build "$BUILD_DIR"

echo
echo "== Copying executables =="
cp "$BUILD_DIR/beamlab_ui.exe" "$DIST_DIR/"
if [[ -f "$BUILD_DIR/beamlab.exe" ]]; then
    cp "$BUILD_DIR/beamlab.exe" "$DIST_DIR/"
fi
cp "$DIST_DIR/beamlab_ui.exe" "$DIST_DIR/BeamLabStudio.exe"

echo
echo "== Copying Qt plugins =="
mkdir -p "$DIST_DIR/platforms" "$DIST_DIR/styles" "$DIST_DIR/imageformats" "$DIST_DIR/iconengines"

if [[ -n "$QT_PLUGIN_ROOT" ]]; then
    [[ -d "$QT_PLUGIN_ROOT/platforms" ]] && cp "$QT_PLUGIN_ROOT/platforms/qwindows.dll" "$DIST_DIR/platforms/" || true
    [[ -d "$QT_PLUGIN_ROOT/styles" ]] && cp "$QT_PLUGIN_ROOT/styles/"*.dll "$DIST_DIR/styles/" 2>/dev/null || true
    [[ -d "$QT_PLUGIN_ROOT/imageformats" ]] && cp "$QT_PLUGIN_ROOT/imageformats/"*.dll "$DIST_DIR/imageformats/" 2>/dev/null || true
    [[ -d "$QT_PLUGIN_ROOT/iconengines" ]] && cp "$QT_PLUGIN_ROOT/iconengines/"*.dll "$DIST_DIR/iconengines/" 2>/dev/null || true
fi

cat > "$DIST_DIR/qt.conf" <<'QTC'
[Paths]
Plugins=.
QTC

echo
echo "== Copying project runtime files =="
mkdir -p "$DIST_DIR/scripts" "$DIST_DIR/tools" "$DIST_DIR/docs"

[[ -d "$PROJECT_ROOT/scripts" ]] && rsync -a "$PROJECT_ROOT/scripts/" "$DIST_DIR/scripts/" --exclude "release/" --exclude "*.bak*" || true
[[ -d "$PROJECT_ROOT/tools" ]] && rsync -a "$PROJECT_ROOT/tools/" "$DIST_DIR/tools/" --exclude "*.bak*" || true
[[ -d "$PROJECT_ROOT/docs" ]] && rsync -a "$PROJECT_ROOT/docs/" "$DIST_DIR/docs/" || true
[[ -f "$PROJECT_ROOT/README.md" ]] && cp "$PROJECT_ROOT/README.md" "$DIST_DIR/"
[[ -f "$PROJECT_ROOT/VERSION" ]] && cp "$PROJECT_ROOT/VERSION" "$DIST_DIR/"

cat > "$DIST_DIR/README_WINDOWS_PORTABLE.txt" <<'TXT'
BeamLabStudio Windows Portable

Run:
  BeamLabStudio.exe

Notes:
- This is a portable Windows build produced from Fedora using MinGW.
- Native ROOT support is disabled in this build.
- COMSOL/CSV workflows are the intended portable demo path.
- If analysis fails, the remaining issue is likely the Linux shell runner script.
TXT

echo
echo "== Resolving DLL dependencies =="
copy_if_exists() {
    local dll="$1"
    local src="$MINGW_BIN/$dll"
    if [[ -f "$src" && ! -f "$DIST_DIR/$dll" ]]; then
        cp "$src" "$DIST_DIR/"
        echo "copied $dll"
    fi
}

scan_and_copy_deps_once() {
    local file="$1"
    x86_64-w64-mingw32-objdump -p "$file" 2>/dev/null \
        | awk '/DLL Name:/ {print $3}' \
        | while read -r dll; do
            copy_if_exists "$dll"
        done
}

# First pass: exes and plugins.
for f in "$DIST_DIR"/*.exe "$DIST_DIR"/platforms/*.dll "$DIST_DIR"/styles/*.dll "$DIST_DIR"/imageformats/*.dll "$DIST_DIR"/iconengines/*.dll; do
    [[ -f "$f" ]] && scan_and_copy_deps_once "$f"
done

# Second/third pass: DLLs copied by previous pass may have their own deps.
for pass in 1 2 3; do
    for f in "$DIST_DIR"/*.dll "$DIST_DIR"/*/*.dll; do
        [[ -f "$f" ]] && scan_and_copy_deps_once "$f"
    done
done

echo
echo "== Package contents =="
find "$DIST_DIR" -maxdepth 2 -type f | sort

echo
echo "== Creating zip =="
mkdir -p "$DIST_ROOT"
(
    cd "$DIST_ROOT"
    zip -r "$(basename "$ZIP_PATH")" "$(basename "$DIST_DIR")" >/dev/null
)

echo
echo "DONE:"
ls -lh "$ZIP_PATH"
