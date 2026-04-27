#!/usr/bin/env python3
import sys
import subprocess
import shutil
from pathlib import Path

def fail(msg):
    print(msg)
    sys.exit(1)

def main():
    if len(sys.argv) < 3:
        print("Uso:")
        print("  run_beamlab_full.py <input_file> <output_dir> [extra args]")
        sys.exit(1)

    input_file = Path(sys.argv[1]).resolve()
    output_dir = Path(sys.argv[2]).resolve()
    extra_args = sys.argv[3:]

    if not input_file.exists():
        fail(f"No existe el archivo de entrada:\n  {input_file}")

    # elegir engine
    if input_file.suffix == ".root":
        engine = Path("build-root/beamlab")
    else:
        engine = Path("build/beamlab")

    engine = engine.resolve()

    if not engine.exists():
        fail(f"No existe el ejecutable:\n  {engine}")

    print(f"[INFO] Ejecutando engine: {engine}")

    cmd = [
        str(engine),
        "--input", str(input_file),
        "--output", str(output_dir),
        *extra_args
    ]

    result = subprocess.run(cmd)
    if result.returncode != 0:
        fail("Falló ejecución de beamlab")

    # === POSTPROCESS LENS ===
    lens_disk = output_dir / "geometry/effective_lens_disk.obj"
    if lens_disk.exists():
        print("[INFO] Generando effective_lens_body...")
        subprocess.run([
            sys.executable,
            "tools/postprocess/build_lens_body_for_run.py",
            str(output_dir),
            "--center-rel", "0.35",
            "--edge-rel", "0.06",
            "--power", "1.85"
        ], check=True)
    else:
        print(f"WARNING: no existe {lens_disk}")

    # === POSTPROCESS TRAJECTORIES ===
    traj_csv = output_dir / "visualization/trajectories_preview.csv"
    if traj_csv.exists():
        print("[INFO] Generando trajectories_preview.obj...")
        subprocess.run([
            sys.executable,
            "tools/postprocess/build_trajectory_obj_for_run.py",
            str(output_dir),
            "--max-points", "200000"
        ], check=True)
    else:
        print(f"WARNING: no existe {traj_csv}")

    print("\nCorrida completa lista:")
    print(f"  {output_dir}")
    print("\nManifest:")
    print(f"  {output_dir}/visualization/visualization_manifest.json")


if __name__ == "__main__":
    main()
