#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import subprocess
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(
        description="Build trajectories_preview.obj for a BeamLabStudio output run."
    )

    parser.add_argument("run_directory", type=Path)
    parser.add_argument("--max-points", type=int, default=200000)

    args = parser.parse_args()

    run = args.run_directory
    csv_path = run / "visualization" / "trajectories_preview.csv"

    if not csv_path.exists():
        raise SystemExit(f"No existe el CSV de trayectorias: {csv_path}")

    converter = Path("tools/converters/trajectory_csv_to_obj.py")

    if not converter.exists():
        raise SystemExit(f"No existe el conversor: {converter}")

    obj_path = run / "geometry" / "trajectories_preview.obj"

    subprocess.run(
        [
            "python3",
            str(converter),
            str(csv_path),
            "--output",
            str(obj_path),
            "--max-points",
            str(args.max_points),
        ],
        check=True,
    )

    manifest_path = run / "visualization" / "visualization_manifest.json"

    if manifest_path.exists():
        data = json.loads(manifest_path.read_text())
        files = data.setdefault("files", {})
        files["trajectories_preview_obj"] = str(obj_path)

        data.setdefault("postprocessing", {})
        data["postprocessing"]["trajectories_preview_obj"] = {
            "enabled": True,
            "source": str(csv_path),
            "output": str(obj_path),
            "max_points": args.max_points,
        }

        manifest_path.write_text(json.dumps(data, indent=2))
        print(f"updated manifest: {manifest_path}")

    print("done")


if __name__ == "__main__":
    main()
