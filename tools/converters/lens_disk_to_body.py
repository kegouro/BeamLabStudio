#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import math
from collections import defaultdict
from pathlib import Path


def parse_obj(path: Path):
    vertices = []
    faces = []

    for raw in path.read_text().splitlines():
        line = raw.strip()

        if not line or line.startswith("#"):
            continue

        parts = line.split()

        if parts[0] == "v" and len(parts) >= 4:
            vertices.append([float(parts[1]), float(parts[2]), float(parts[3])])

        elif parts[0] == "f" and len(parts) >= 4:
            face = []

            for token in parts[1:]:
                index_text = token.split("/")[0]
                idx = int(index_text)

                if idx < 0:
                    idx = len(vertices) + idx
                else:
                    idx = idx - 1

                face.append(idx)

            if len(face) >= 3:
                faces.append(face)

    return vertices, faces


def write_obj(path: Path, vertices, faces, comments=None):
    path.parent.mkdir(parents=True, exist_ok=True)

    with path.open("w") as f:
        if comments:
            for comment in comments:
                f.write(f"# {comment}\n")

        f.write("o effective_lens_body\n")

        for v in vertices:
            f.write(f"v {v[0]:.12e} {v[1]:.12e} {v[2]:.12e}\n")

        for face in faces:
            one_based = [str(i + 1) for i in face]
            f.write("f " + " ".join(one_based) + "\n")


def bbox(vertices):
    mins = [min(v[i] for v in vertices) for i in range(3)]
    maxs = [max(v[i] for v in vertices) for i in range(3)]
    return mins, maxs


def face_edges(face):
    for i, a in enumerate(face):
        b = face[(i + 1) % len(face)]
        yield a, b


def boundary_edges(faces):
    counts = defaultdict(int)
    directed = {}

    for face in faces:
        for a, b in face_edges(face):
            key = tuple(sorted((a, b)))
            counts[key] += 1
            directed[key] = (a, b)

    return [directed[key] for key, count in counts.items() if count == 1]


def fallback_boundary_from_radius(vertices, radial_axes, center, rhos, threshold=0.985):
    candidates = [i for i, rho in enumerate(rhos) if rho >= threshold]

    if len(candidates) < 3:
        candidates = sorted(range(len(vertices)), key=lambda i: rhos[i], reverse=True)[:64]

    a0, a1 = radial_axes

    def angle(i):
        return math.atan2(vertices[i][a1] - center[a1], vertices[i][a0] - center[a0])

    candidates.sort(key=angle)

    edges = []
    for i, a in enumerate(candidates):
        b = candidates[(i + 1) % len(candidates)]
        if a != b:
            edges.append((a, b))

    return edges


def main():
    parser = argparse.ArgumentParser(
        description="Convert a flat effective lens disk OBJ into a closed biconvex effective lens body."
    )

    parser.add_argument("input_obj", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--preview-output", type=Path)
    parser.add_argument("--equation-output", type=Path)
    parser.add_argument("--metadata-output", type=Path)

    parser.add_argument("--center-thickness", type=float, default=None)
    parser.add_argument("--edge-thickness", type=float, default=None)
    parser.add_argument("--center-rel", type=float, default=0.35)
    parser.add_argument("--edge-rel", type=float, default=0.06)
    parser.add_argument("--power", type=float, default=1.85)

    args = parser.parse_args()

    vertices, faces = parse_obj(args.input_obj)

    if not vertices:
        raise SystemExit(f"No vertices found in {args.input_obj}")

    if not faces:
        raise SystemExit(f"No faces found in {args.input_obj}")

    mins, maxs = bbox(vertices)
    extents = [maxs[i] - mins[i] for i in range(3)]

    normal_axis = min(range(3), key=lambda i: extents[i])
    radial_axes = [i for i in range(3) if i != normal_axis]

    center = [
        sum(v[i] for v in vertices) / len(vertices)
        for i in range(3)
    ]

    radial_distances = []
    a0, a1 = radial_axes

    for v in vertices:
        da = v[a0] - center[a0]
        db = v[a1] - center[a1]
        radial_distances.append(math.sqrt(da * da + db * db))

    aperture_radius = max(radial_distances) if radial_distances else 1.0

    if aperture_radius <= 0:
        aperture_radius = 1.0

    center_thickness = (
        args.center_thickness
        if args.center_thickness is not None
        else args.center_rel * aperture_radius
    )

    edge_thickness = (
        args.edge_thickness
        if args.edge_thickness is not None
        else args.edge_rel * aperture_radius
    )

    if center_thickness <= edge_thickness:
        center_thickness = edge_thickness * 2.0

    rhos = [min(1.0, r / aperture_radius) for r in radial_distances]

    body_vertices = []

    for side_sign in (-1.0, 1.0):
        for v, rho in zip(vertices, rhos):
            curved = max(0.0, 1.0 - rho * rho)
            thickness = edge_thickness + (center_thickness - edge_thickness) * (curved ** args.power)
            half = 0.5 * thickness

            out = list(v)
            out[normal_axis] = center[normal_axis] + side_sign * half
            body_vertices.append(out)

    n = len(vertices)
    body_faces = []

    for face in faces:
        body_faces.append(list(face))

    for face in faces:
        body_faces.append([idx + n for idx in reversed(face)])

    edges = boundary_edges(faces)

    if not edges:
        edges = fallback_boundary_from_radius(vertices, radial_axes, center, rhos)

    for a, b in edges:
        body_faces.append([a, b, b + n])
        body_faces.append([a, b + n, a + n])

    comments = [
        "BeamLabStudio effective lens body",
        "Generated from the exported effective_lens_disk OBJ",
        "This is an effective geometric proxy, not a unique inverse reconstruction of the physical magnet.",
        f"input_obj={args.input_obj}",
        f"normal_axis={normal_axis}",
        f"radial_axes={radial_axes}",
        f"aperture_radius_m={aperture_radius:.12e}",
        f"center_thickness_m={center_thickness:.12e}",
        f"edge_thickness_m={edge_thickness:.12e}",
        f"sag_power={args.power:.12e}",
    ]

    write_obj(args.output, body_vertices, body_faces, comments)

    if args.preview_output:
        write_obj(args.preview_output, body_vertices, body_faces, comments)

    if args.equation_output:
        args.equation_output.parent.mkdir(parents=True, exist_ok=True)

        axis_names = ["x", "y", "z"]
        normal_name = axis_names[normal_axis]
        radial_names = [axis_names[i] for i in radial_axes]

        args.equation_output.write_text(
            "\n".join([
                "Effective biconvex lens body parametrization",
                "",
                "This geometry is generated from the exported effective_lens_disk OBJ.",
                "It is an effective geometric proxy based on the beam focal aperture.",
                "",
                "Let rho in [0,1] and theta in [0, 2*pi).",
                "Let R(theta) be the boundary radius sampled from the exported disk aperture.",
                "",
                "Front surface:",
                "X_front(rho, theta) = C + rho R(theta) [cos(theta) u_hat + sin(theta) v_hat] - h(rho) n_hat",
                "",
                "Back surface:",
                "X_back(rho, theta) = C + rho R(theta) [cos(theta) u_hat + sin(theta) v_hat] + h(rho) n_hat",
                "",
                "Half-thickness:",
                "h(rho) = 0.5 * [ t_edge + (t_center - t_edge) * (1 - rho^2)^p ]",
                "",
                f"normal_axis = {normal_name}",
                f"radial_axes = {radial_names[0]}, {radial_names[1]}",
                f"aperture_radius_m = {aperture_radius:.12e}",
                f"t_center_m = {center_thickness:.12e}",
                f"t_edge_m = {edge_thickness:.12e}",
                f"p = {args.power:.12e}",
                "",
            ])
        )

    metadata = {
        "schema_version": "0.1",
        "input_obj": str(args.input_obj),
        "output_obj": str(args.output),
        "normal_axis": normal_axis,
        "radial_axes": radial_axes,
        "aperture_radius_m": aperture_radius,
        "center_thickness_m": center_thickness,
        "edge_thickness_m": edge_thickness,
        "sag_power": args.power,
        "input_vertices": len(vertices),
        "input_faces": len(faces),
        "output_vertices": len(body_vertices),
        "output_faces": len(body_faces),
        "boundary_edges": len(edges),
    }

    if args.metadata_output:
        args.metadata_output.parent.mkdir(parents=True, exist_ok=True)
        args.metadata_output.write_text(json.dumps(metadata, indent=2))

    print("effective_lens_body generated")
    print(f"input vertices:  {len(vertices)}")
    print(f"input faces:     {len(faces)}")
    print(f"output vertices: {len(body_vertices)}")
    print(f"output faces:    {len(body_faces)}")
    print(f"boundary edges:  {len(edges)}")
    print(f"normal axis:     {normal_axis}")
    print(f"aperture radius: {aperture_radius:.6e} m")
    print(f"center thickness:{center_thickness:.6e} m")
    print(f"edge thickness:  {edge_thickness:.6e} m")


if __name__ == "__main__":
    main()
