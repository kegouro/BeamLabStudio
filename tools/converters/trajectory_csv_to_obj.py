#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
from collections import defaultdict
from pathlib import Path


def evenly_spaced_indices(count: int, max_count: int) -> list[int]:
    if count <= 0 or max_count <= 0:
        return []

    selected_count = min(count, max_count)

    if selected_count == 1:
        return [0]

    return [
        i * (count - 1) // (selected_count - 1)
        for i in range(selected_count)
    ]


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

    raw_total_points = sum(len(points) for points in groups.values())

    ordered_groups = [
        (trajectory_id, points)
        for trajectory_id, points in sorted(groups.items())
        if len(points) >= 2
    ]

    if args.max_points < 2:
        ordered_groups = []
    elif len(ordered_groups) * 2 > args.max_points:
        group_indices = evenly_spaced_indices(len(ordered_groups), args.max_points // 2)
        ordered_groups = [ordered_groups[i] for i in group_indices]

    selected_points = sum(len(points) for _, points in ordered_groups)
    samples_per_trajectory = (
        max(2, args.max_points // len(ordered_groups))
        if ordered_groups
        else 0
    )

    args.output.parent.mkdir(parents=True, exist_ok=True)

    vertex_index = 1
    written_vertices = 0
    written_lines = 0

    with args.output.open("w") as out:
        out.write("# BeamLabStudio trajectory preview OBJ\n")
        out.write("# Uses OBJ polyline records: l i j k ...\n")
        out.write("o trajectories_preview\n")

        line_records = []

        for trajectory_id, points in ordered_groups:
            indices = []

            for point_index in evenly_spaced_indices(len(points), samples_per_trajectory):
                _, x, y, z = points[point_index]
                out.write(f"v {x:.12e} {y:.12e} {z:.12e}\n")
                indices.append(vertex_index)
                vertex_index += 1
                written_vertices += 1

            if len(indices) >= 2:
                line_records.append(indices)

        for indices in line_records:
            out.write("l " + " ".join(str(i) for i in indices) + "\n")
            written_lines += 1

    print("trajectory OBJ generated")
    print(f"input groups:      {len(groups)}")
    print(f"output groups:     {len(ordered_groups)}")
    print(f"input points:      {raw_total_points}")
    print(f"selected points:   {selected_points}")
    print(f"output vertices:   {written_vertices}")
    print(f"output polylines:  {written_lines}")
    print(f"samples/polyline:  {samples_per_trajectory}")
    print(f"output:            {args.output}")


if __name__ == "__main__":
    main()
