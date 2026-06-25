#!/bin/bash
# BeamLabStudio — lanzador ligero.
# Abre el .app ya construido SIN recompilar. Doble-clic en Finder, o ./BeamLabStudio.command
# (Para recompilar tras cambios de código usa ./launch_beamlabstudio.sh)
set -e
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Toma el .app más reciente entre los directorios de build (build-qt, build-mac, build-release, build…).
APP="$(ls -dt "$DIR"/build*/src/ui/BeamLabStudio.app 2>/dev/null | head -1)"

if [ -z "$APP" ] || [ ! -d "$APP" ]; then
  echo "No hay BeamLabStudio.app construido todavía. Construyendo (solo esta vez)…"
  cmake -B "$DIR/build-qt" -S "$DIR" \
        -DCMAKE_BUILD_TYPE=Release -DBEAMLAB_ENABLE_QT_UI=ON \
        -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt >/dev/null
  cmake --build "$DIR/build-qt" -j"$(sysctl -n hw.ncpu)"
  APP="$DIR/build-qt/src/ui/BeamLabStudio.app"
fi

echo "Abriendo $APP"
open "$APP"
