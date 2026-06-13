#!/bin/bash
# Empaqueta BeamLabStudio.app autocontenido + BeamLabStudio.dmg (sin firmar).
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${PROJECT_DIR}/build-release"
QT_PREFIX="$(brew --prefix qt)"
APP="${BUILD_DIR}/src/ui/BeamLabStudio.app"
DMG="${PROJECT_DIR}/BeamLabStudio.dmg"

echo "[package] Configurando Release…"
cmake -B "${BUILD_DIR}" -S "${PROJECT_DIR}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBEAMLAB_ENABLE_QT_UI=ON \
    -DCMAKE_PREFIX_PATH="${QT_PREFIX}"

echo "[package] Compilando beamlab_ui…"
cmake --build "${BUILD_DIR}" --target beamlab_ui -j"$(sysctl -n hw.ncpu)"

echo "[package] Embebiendo frameworks Qt con macdeployqt…"
"${QT_PREFIX}/bin/macdeployqt" "${APP}" -always-overwrite

echo "[package] Verificando dependencias del binario…"
otool -L "${APP}/Contents/MacOS/BeamLabStudio" | grep -i qt || true

echo "[package] Creando DMG…"
rm -f "${DMG}"
STAGING="$(mktemp -d)"
cp -R "${APP}" "${STAGING}/"
ln -s /Applications "${STAGING}/Applications"
hdiutil create -volname "BeamLabStudio" -srcfolder "${STAGING}" \
    -ov -format UDZO "${DMG}"
rm -rf "${STAGING}"

echo "[package] Listo:"
echo "  App: ${APP}"
echo "  DMG: ${DMG}"
