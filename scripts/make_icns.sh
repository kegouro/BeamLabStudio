#!/bin/bash
# Genera resources/macos/AppIcon.icns desde el SVG de marca.
# Requiere: rsvg-convert o qlmanage/sips para rasterizar; iconutil (nativo macOS).
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SVG="${PROJECT_DIR}/src/ui/qt/assets/branding/beamlabstudio_icon.svg"
OUT="${PROJECT_DIR}/resources/macos/AppIcon.icns"

# Early validation: check if SVG exists
if [ ! -f "${SVG}" ]; then
    echo "[make_icns] ERROR: SVG no encontrado: ${SVG}" >&2
    exit 1
fi

WORK="$(mktemp -d)"
ICONSET="${WORK}/AppIcon.iconset"
mkdir -p "${ICONSET}"

# Rasteriza un PNG base de 1024px. Preferimos rsvg-convert; si no, qlmanage.
BASE="${WORK}/base_1024.png"
if command -v rsvg-convert >/dev/null 2>&1; then
    rsvg-convert -w 1024 -h 1024 "${SVG}" -o "${BASE}"
else
    qlmanage -t -s 1024 -o "${WORK}" "${SVG}" >/dev/null 2>&1
    produced="$(find "${WORK}" -maxdepth 1 -name '*.png' ! -name 'base_1024.png' | head -n1)"
    if [ -z "${produced}" ]; then
        echo "[make_icns] ERROR: qlmanage no produjo PNG" >&2
        exit 1
    fi
    mv "${produced}" "${BASE}"
fi

# Post-rasterization check: verify base PNG was created and is non-empty
if [ ! -s "${BASE}" ]; then
    echo "[make_icns] ERROR: rasterización falló" >&2
    exit 1
fi

# Escala a todos los tamaños requeridos por iconutil.
for size in 16 32 64 128 256 512 1024; do
    sips -z "${size}" "${size}" "${BASE}" --out "${ICONSET}/icon_${size}x${size}.png" >/dev/null
done
# Variantes @2x (retina): icon_NxN@2x = 2*N px.
cp "${ICONSET}/icon_32x32.png"   "${ICONSET}/icon_16x16@2x.png"
cp "${ICONSET}/icon_64x64.png"   "${ICONSET}/icon_32x32@2x.png"
cp "${ICONSET}/icon_256x256.png" "${ICONSET}/icon_128x128@2x.png"
cp "${ICONSET}/icon_512x512.png" "${ICONSET}/icon_256x256@2x.png"
cp "${ICONSET}/icon_1024x1024.png" "${ICONSET}/icon_512x512@2x.png"

mkdir -p "$(dirname "${OUT}")"
iconutil -c icns "${ICONSET}" -o "${OUT}"
echo "[make_icns] Generado: ${OUT}"
rm -rf "${WORK}"
