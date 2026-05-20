#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if [ "$#" -lt 2 ]; then
  echo "Uso:"
  echo "  scripts/run_beamlab_full.sh <input_file> <output_dir> [opciones extra para beamlab]"
  echo ""
  echo "Ejemplo:"
  echo "  scripts/run_beamlab_full.sh \\"
  echo "    stepping_data.csv \\"
  echo "    outputs/mi_corrida \\"
  echo "    --axis z --reference-mode axial-bins --binning equal-count --axial-bins 501 --window 5"
  exit 1
fi

INPUT="$1"
OUTPUT="$2"
shift 2

if [ ! -f "$INPUT" ]; then
  echo "No existe el archivo de entrada:"
  echo "  $INPUT"
  exit 1
fi

if [[ "$INPUT" == *.root ]]; then
  ENGINE="${SCRIPT_DIR}/build-root/beamlab"
else
  ENGINE="${SCRIPT_DIR}/build/beamlab"
fi

if [ ! -x "$ENGINE" ]; then
  echo "No existe el ejecutable:"
  echo "  $ENGINE"
  echo ""
  echo "Compila primero:"
  echo "  cmake -S . -B build -G Ninja"
  echo "  cmake --build build"
  echo ""
  echo "o para ROOT:"
  echo "  cmake -S . -B build-root -G Ninja -DBEAMLAB_ENABLE_ROOT=ON"
  echo "  cmake --build build-root"
  exit 1
fi

echo "Analysis engine: ${ENGINE}"
echo "Input file:      ${INPUT}"
echo "Output dir:      ${OUTPUT}"
echo ""

"${ENGINE}" \
  --input "${INPUT}" \
  --output "${OUTPUT}" \
  "$@"

if [ -f "$OUTPUT/geometry/effective_lens_disk.obj" ]; then
  python3 tools/postprocess/build_lens_body_for_run.py \
    "$OUTPUT" \
    --center-rel 0.35 \
    --edge-rel 0.06 \
    --power 1.85
else
  echo "WARNING: no existe $OUTPUT/geometry/effective_lens_disk.obj"
  echo "No se generó effective_lens_body."
fi

if [ -f "$OUTPUT/visualization/trajectories_preview.csv" ]; then
  python3 tools/postprocess/build_trajectory_obj_for_run.py \
    "$OUTPUT" \
    --max-points 200000
else
  echo "WARNING: no existe $OUTPUT/visualization/trajectories_preview.csv"
  echo "No se generó trajectories_preview.obj."
fi

echo ""
echo "Corrida completa lista:"
echo "  $OUTPUT"
echo ""
echo "Manifest para abrir en la UI:"
echo "  $OUTPUT/visualization/visualization_manifest.json"
