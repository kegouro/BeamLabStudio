#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import subprocess
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(
        description="Build effective_lens_body files for a BeamLabStudio output run."
    )

    parser.add_argument("run_directory", type=Path)
    parser.add_argument("--center-rel", type=float, default=0.35)
    parser.add_argument("--edge-rel", type=float, default=0.06)
    parser.add_argument("--power", type=float, default=1.85)

    args = parser.parse_args()

    run = args.run_directory
    disk_obj = run / "geometry" / "effective_lens_disk.obj"

    if not disk_obj.exists():
        raise SystemExit(f"No existe el disco base: {disk_obj}")

    converter = Path("tools/converters/lens_disk_to_body.py")

    if not converter.exists():
        raise SystemExit(f"No existe el conversor: {converter}")

    body_obj = run / "geometry" / "effective_lens_body.obj"
    body_preview_obj = run / "geometry" / "effective_lens_body_preview.obj"
    equation_txt = run / "equations" / "effective_lens_body_parametric_equation.txt"
    metadata_json = run / "reports" / "effective_lens_body_metadata.json"

    cmd = [
        "python3",
        str(converter),
        str(disk_obj),
        "--output",
        str(body_obj),
        "--preview-output",
        str(body_preview_obj),
        "--equation-output",
        str(equation_txt),
        "--metadata-output",
        str(metadata_json),
        "--center-rel",
        str(args.center_rel),
        "--edge-rel",
        str(args.edge_rel),
        "--power",
        str(args.power),
    ]

    subprocess.run(cmd, check=True)

    manifest_path = run / "visualization" / "visualization_manifest.json"

    if manifest_path.exists():
        data = json.loads(manifest_path.read_text())
        files = data.setdefault("files", {})

        files["effective_lens_disk_obj"] = str(run / "geometry" / "effective_lens_disk.obj")
        files["effective_lens_disk_preview_obj"] = str(run / "geometry" / "effective_lens_disk_preview.obj")

        files["effective_lens_body_obj"] = str(body_obj)
        files["effective_lens_body_preview_obj"] = str(body_preview_obj)
        files["effective_lens_body_parametric"] = str(equation_txt)

        data.setdefault("postprocessing", {})
        data["postprocessing"]["effective_lens_body"] = {
            "enabled": True,
            "source": str(disk_obj),
            "output": str(body_obj),
            "preview_output": str(body_preview_obj),
            "center_rel": args.center_rel,
            "edge_rel": args.edge_rel,
            "power": args.power,
        }

        manifest_path.write_text(json.dumps(data, indent=2))
        print(f"updated manifest: {manifest_path}")
    else:
        print(f"manifest not found, skipped manifest update: {manifest_path}")

    print("done")


if __name__ == "__main__":
    main()
