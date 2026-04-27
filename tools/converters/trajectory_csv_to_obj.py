#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
from collections import defaultdict
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(
        description="Convert BeamLabStudio trajectories_preview.csv into an OBJ line cloud."
    )

    parser.add_argument("input_csv", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--max-points", type=int, default=200000)

    args = parser.parse_args()

    if not args.input_csv.exists():
        raise SystemExit(f"No existe el CSV: {args.input_csv}")

    groups = defaultdict(list)

    with args.input_csv.open(newline="") as f:
        filtered = (line for line in f if not line.lstrip().startswith("#"))
        reader = csv.DictReader(filtered)

        for row in reader:
            try:
                trajectory_index = int(row["trajectory_index"])
                sample_index = int(row["sample_index"])
                x = float(row["x_m"])
                y = float(row["y_m"])
                z = float(row["z_m"])
            except Exception:
                continue

            groups[trajectory_index].append((sample_index, x, y, z))

    for key in list(groups.keys()):
        groups[key].sort(key=lambda item: item[0])

    total_points = sum(len(points) for points in groups.values())

    stride = 1
    if total_points > args.max_points:
        stride = max(1, total_points // args.max_points)

    args.output.parent.mkdir(parents=True, exist_ok=True)

    vertex_index = 1
    written_vertices = 0
    written_lines = 0

    with args.output.open("w") as out:
        out.write("# BeamLabStudio trajectory preview OBJ\n")
        out.write("# Uses OBJ polyline records: l i j k ...\n")
        out.write("o trajectories_preview\n")

        line_records = []

        global_counter = 0

        for trajectory_id, points in groups.items():
            indices = []

            for _, x, y, z in points:
                if global_counter % stride == 0:
                    out.write(f"v {x:.12e} {y:.12e} {z:.12e}\n")
                    indices.append(vertex_index)
                    vertex_index += 1
                    written_vertices += 1

                global_counter += 1

            if len(indices) >= 2:
                line_records.append(indices)

        for indices in line_records:
            out.write("l " + " ".join(str(i) for i in indices) + "\n")
            written_lines += 1

    print("trajectory OBJ generated")
    print(f"input groups:      {len(groups)}")
    print(f"input points:      {total_points}")
    print(f"output vertices:   {written_vertices}")
    print(f"output polylines:  {written_lines}")
    print(f"stride:            {stride}")
    print(f"output:            {args.output}")


if __name__ == "__main__":
    main()
