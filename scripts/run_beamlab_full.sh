#!/bin/bash
set -euo pipefail

# --- Robustez para paths con espacios, paréntesis, etc. ---
IFS=$' \t\n'

# --- Argumentos posicionales (la UI los pasa en este orden) ---
ENGINE="${1:-beamlab}"
INPUT="$2"
OUTPUT="$3"
shift 3

# --- Validaciones ---
if [ -z "$ENGINE" ]; then
    echo "ERROR: ENGINE no proporcionado" >&2
    exit 1
fi

if [ ! -f "$INPUT" ]; then
    echo "ERROR: Archivo input no existe: $INPUT" >&2
    exit 1
fi

mkdir -p "$OUTPUT"

# --- Debug (quítalo después de verificar que funciona) ---
echo "DEBUG ENGINE=$ENGINE"
echo "DEBUG INPUT=$INPUT"
echo "DEBUG OUTPUT=$OUTPUT"
echo "DEBUG EXTRA_ARGS=$*"

# --- Ejecución real ---
"$ENGINE" --input "$INPUT" --output "$OUTPUT" "$@"
