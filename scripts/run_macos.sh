#!/bin/bash
# ─────────────────────────────────────────────────────────────────────────
#  run_macos.sh — compila, abre y prueba BeamLabStudio.app en macOS (arm64)
#
#  Uso:
#    ./scripts/run_macos.sh              # compila build-mac y abre el .app
#    ./scripts/run_macos.sh --test       # corre tests unitarios + regresión CLI, luego abre
#    ./scripts/run_macos.sh --package     # build autocontenido (macdeployqt) + DMG, abre ese
#    ./scripts/run_macos.sh --smoke       # lanza, confirma que arranca, y cierra (sin dejar ventana)
#    ./scripts/run_macos.sh --no-open     # solo compila, no abre
#    ./scripts/run_macos.sh --help
# ─────────────────────────────────────────────────────────────────────────
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${PROJECT_DIR}/build-mac"
JOBS="$(sysctl -n hw.ncpu)"

DO_TEST=false
DO_PACKAGE=false
DO_SMOKE=false
DO_OPEN=true

for arg in "$@"; do
    case "${arg}" in
        --test)    DO_TEST=true ;;
        --package) DO_PACKAGE=true ;;
        --smoke)   DO_SMOKE=true ;;
        --no-open) DO_OPEN=false ;;
        --help|-h)
            sed -n '3,12p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
            exit 0 ;;
        *)
            echo "[run_macos] Opción desconocida: ${arg} (usa --help)" >&2
            exit 1 ;;
    esac
done

# ── Prerrequisitos ────────────────────────────────────────────────────────
if ! command -v brew >/dev/null 2>&1; then
    echo "[run_macos] ERROR: Homebrew no encontrado. Instala Qt con 'brew install qt'." >&2
    exit 1
fi
QT_PREFIX="$(brew --prefix qt 2>/dev/null || true)"
if [ -z "${QT_PREFIX}" ] || [ ! -d "${QT_PREFIX}" ]; then
    echo "[run_macos] ERROR: Qt no encontrado. Ejecuta 'brew install qt'." >&2
    exit 1
fi

# ── Camino --package: bundle autocontenido + DMG ──────────────────────────
if [ "${DO_PACKAGE}" = true ]; then
    echo "[run_macos] Empaquetando bundle autocontenido + DMG…"
    "${PROJECT_DIR}/scripts/package_macos.sh"
    APP="${PROJECT_DIR}/build-release/src/ui/BeamLabStudio.app"
else
    # ── Configura build-mac si hace falta ─────────────────────────────────
    if [ ! -f "${BUILD_DIR}/CMakeCache.txt" ]; then
        echo "[run_macos] Configurando build-mac (Release + Qt UI)…"
        cmake -B "${BUILD_DIR}" -S "${PROJECT_DIR}" \
            -DCMAKE_BUILD_TYPE=Release \
            -DBEAMLAB_ENABLE_QT_UI=ON \
            -DCMAKE_PREFIX_PATH="${QT_PREFIX}"
    fi

    # ── Tests opcionales (antes de compilar la UI, para fallar rápido) ────
    if [ "${DO_TEST}" = true ]; then
        echo "[run_macos] Compilando y corriendo tests unitarios…"
        cmake --build "${BUILD_DIR}" --target beamlab_tests -j"${JOBS}"
        "${BUILD_DIR}/tests/unit/beamlab_tests"

        echo "[run_macos] Regresión CLI con tests/muon_tracks.csv…"
        if [ -f "${PROJECT_DIR}/tests/muon_tracks.csv" ]; then
            "${BUILD_DIR}/beamlab" -i "${PROJECT_DIR}/tests/muon_tracks.csv" \
                -o /tmp/beamlab_run_macos_check 2>&1 \
                | grep -iE "focus index|axial position|confidence=" || true
        else
            echo "[run_macos] (omito regresión CLI: tests/muon_tracks.csv no presente)"
        fi
    fi

    echo "[run_macos] Compilando beamlab_ui…"
    cmake --build "${BUILD_DIR}" --target beamlab_ui -j"${JOBS}"
    APP="${BUILD_DIR}/src/ui/BeamLabStudio.app"
fi

if [ ! -d "${APP}" ]; then
    echo "[run_macos] ERROR: no se encontró el bundle en ${APP}" >&2
    exit 1
fi
echo "[run_macos] App lista: ${APP}"

# ── Smoke test: lanza, confirma arranque, cierra ──────────────────────────
if [ "${DO_SMOKE}" = true ]; then
    echo "[run_macos] Smoke test…"
    open "${APP}"
    sleep 4
    if pgrep -f "BeamLabStudio.app" >/dev/null; then
        echo "[run_macos] ✓ RUNNING — la app arrancó sin crash."
        pkill -f "BeamLabStudio.app" 2>/dev/null || true
        exit 0
    else
        echo "[run_macos] ✗ NOT RUNNING — la app no quedó viva." >&2
        exit 1
    fi
fi

# ── Abrir la app para uso interactivo ─────────────────────────────────────
if [ "${DO_OPEN}" = true ]; then
    echo "[run_macos] Abriendo BeamLabStudio…"
    echo "[run_macos]   (primera vez por Gatekeeper: clic-derecho ▸ Abrir si macOS lo bloquea)"
    open "${APP}"
else
    echo "[run_macos] Compilación lista (--no-open: no se abrió la app)."
fi
