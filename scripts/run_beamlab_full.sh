#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -lt 3 ]; then
  echo "Uso:"
  echo "  scripts/run_beamlab_full.sh <engine_path> <input_file> <output_dir> [opciones extra]"
  echo ""
  echo "Ejemplo:"
  echo "  scripts/run_beamlab_full.sh \\"
  echo "    ./build/beamlab \\"
  echo "    stepping_data.csv \\"
  echo "    outputs/mi_corrida \\"
  echo "    --axis z --reference-mode axial-bins --binning equal-count --axial-bins 501 --window 5"
  exit 1
fi

ENGINE="$1"
INPUT="$2"
OUTPUT="$3"
shift 3

if [ -z "${ENGINE}" ] || [ ! -f "${ENGINE}" ]; then
  echo "Error: engine not found or empty: '${ENGINE}'"
  exit 1
fi

if [ ! -f "${INPUT}" ]; then
  echo "Error: input file not found: ${INPUT}"
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
