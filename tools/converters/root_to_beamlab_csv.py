#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import math
import sys
from pathlib import Path
from typing import Dict, Iterable, Optional


CANONICAL_COLUMNS = [
    "x_cm",
    "y_cm",
    "z_cm",
    "edep_MeV",
    "kinE_MeV",
    "momx_MeV",
    "momy_MeV",
    "momz_MeV",
    "time_ns",
    "trackID",
    "eventID",
]

REQUIRED_COLUMNS = ["x_cm", "y_cm", "z_cm", "time_ns", "trackID", "eventID"]

ALIASES = {
    "x_cm": ["x_cm", "x", "posx", "position_x", "prex", "postx"],
    "y_cm": ["y_cm", "y", "posy", "position_y", "prey", "posty"],
    "z_cm": ["z_cm", "z", "posz", "position_z", "prez", "postz"],
    "edep_MeV": ["edep_mev", "edep", "energydeposit", "energy_deposit", "de"],
    "kinE_MeV": ["kine_mev", "kine", "kineticenergy", "kinetic_energy", "ekin"],
    "momx_MeV": ["momx_mev", "momx", "px", "momentum_x"],
    "momy_MeV": ["momy_mev", "momy", "py", "momentum_y"],
    "momz_MeV": ["momz_mev", "momz", "pz", "momentum_z"],
    "time_ns": ["time_ns", "time", "globaltime", "global_time", "t"],
    "trackID": ["trackid", "track_id", "track"],
    "eventID": ["eventid", "event_id", "event"],
}


def normalize(name: str) -> str:
    return (
        name.strip()
        .replace(" ", "")
        .replace("-", "_")
        .replace(".", "_")
        .replace("/", "_")
        .lower()
    )


def import_uproot():
    try:
        import uproot  # type: ignore
        return uproot
    except Exception as exc:
        print(
            "ERROR: falta uproot. Instálalo con:\n"
            "  python3 -m venv .venv\n"
            "  source .venv/bin/activate\n"
            "  python -m pip install uproot awkward numpy\n",
            file=sys.stderr,
        )
        raise SystemExit(2) from exc


def find_tree(root_file, requested_tree: Optional[str]):
    if requested_tree:
        try:
            return root_file[requested_tree], requested_tree
        except Exception as exc:
            raise SystemExit(f"No pude abrir el árbol ROOT '{requested_tree}': {exc}") from exc

    candidates = []

    for key in root_file.keys(cycle=False):
        try:
            obj = root_file[key]
        except Exception:
            continue

        if hasattr(obj, "keys") and hasattr(obj, "num_entries"):
            candidates.append((key, obj))

    if not candidates:
        raise SystemExit("No encontré ningún TTree/TBranch-like object dentro del archivo ROOT.")

    return candidates[0][1], candidates[0][0]


def list_root_file(path: Path, tree_name: Optional[str]) -> None:
    uproot = import_uproot()

    with uproot.open(path) as root_file:
        print(f"ROOT file: {path}")
        print("\nObjects:")

        for key in root_file.keys(cycle=False):
            print(f"  - {key}")

        tree, resolved_name = find_tree(root_file, tree_name)
        print(f"\nSelected tree: {resolved_name}")
        print(f"Entries: {tree.num_entries}")
        print("\nBranches:")

        for branch in tree.keys():
            print(f"  - {branch}")


def resolve_branches(branch_names: Iterable[str]) -> Dict[str, Optional[str]]:
    original_by_normalized = {normalize(name): name for name in branch_names}
    mapping: Dict[str, Optional[str]] = {}

    for canonical in CANONICAL_COLUMNS:
        found = None

        for alias in ALIASES[canonical]:
            key = normalize(alias)

            if key in original_by_normalized:
                found = original_by_normalized[key]
                break

        mapping[canonical] = found

    return mapping


def position_scale_to_cm(unit: str) -> float:
    unit = unit.lower()

    if unit == "cm":
        return 1.0
    if unit == "mm":
        return 0.1
    if unit == "m":
        return 100.0

    raise SystemExit(f"Unidad de posición no soportada: {unit}. Usa cm, mm o m.")


def time_scale_to_ns(unit: str) -> float:
    unit = unit.lower()

    if unit == "ns":
        return 1.0
    if unit == "s":
        return 1.0e9
    if unit == "us":
        return 1.0e3
    if unit == "ms":
        return 1.0e6

    raise SystemExit(f"Unidad de tiempo no soportada: {unit}. Usa ns, us, ms o s.")


def safe_value(arrays, branch: Optional[str], row: int, default: float = 0.0):
    if branch is None:
        return default

    value = arrays[branch][row]

    try:
        if hasattr(value, "tolist"):
            value = value.tolist()
    except Exception:
        pass

    if isinstance(value, (list, tuple)):
        if not value:
            return default
        value = value[0]

    return value


def convert_root_to_csv(args) -> None:
    uproot = import_uproot()

    input_path = Path(args.input).expanduser().resolve()
    output_path = Path(args.output).expanduser().resolve()
    output_path.parent.mkdir(parents=True, exist_ok=True)

    pos_scale = position_scale_to_cm(args.position_unit)
    time_scale = time_scale_to_ns(args.time_unit)

    with uproot.open(input_path) as root_file:
        tree, tree_name = find_tree(root_file, args.tree)
        branch_names = list(tree.keys())
        mapping = resolve_branches(branch_names)

        missing_required = [name for name in REQUIRED_COLUMNS if mapping[name] is None]
        if missing_required:
            print("No pude resolver columnas requeridas:", ", ".join(missing_required), file=sys.stderr)
            print("\nBranches disponibles:", file=sys.stderr)
            for branch in branch_names:
                print(f"  - {branch}", file=sys.stderr)
            raise SystemExit(1)

        selected_branches = sorted({branch for branch in mapping.values() if branch is not None})

        print(f"Input ROOT: {input_path}")
        print(f"Tree: {tree_name}")
        print(f"Entries: {tree.num_entries}")
        print(f"Output CSV: {output_path}")
        print("\nBranch mapping:")
        for canonical in CANONICAL_COLUMNS:
            print(f"  {canonical:10s} <- {mapping[canonical]}")

        written = 0

        with output_path.open("w", newline="") as f:
            writer = csv.writer(f)
            writer.writerow(CANONICAL_COLUMNS)

            for arrays in tree.iterate(
                selected_branches,
                step_size=args.step_size,
                library="np",
            ):
                batch_len = len(next(iter(arrays.values())))

                for i in range(batch_len):
                    if args.max_rows is not None and written >= args.max_rows:
                        print(f"\nWrote {written} rows.")
                        return

                    x_cm = float(safe_value(arrays, mapping["x_cm"], i)) * pos_scale
                    y_cm = float(safe_value(arrays, mapping["y_cm"], i)) * pos_scale
                    z_cm = float(safe_value(arrays, mapping["z_cm"], i)) * pos_scale
                    time_ns = float(safe_value(arrays, mapping["time_ns"], i)) * time_scale

                    row = [
                        x_cm,
                        y_cm,
                        z_cm,
                        float(safe_value(arrays, mapping["edep_MeV"], i, 0.0)),
                        float(safe_value(arrays, mapping["kinE_MeV"], i, 0.0)),
                        float(safe_value(arrays, mapping["momx_MeV"], i, 0.0)),
                        float(safe_value(arrays, mapping["momy_MeV"], i, 0.0)),
                        float(safe_value(arrays, mapping["momz_MeV"], i, 0.0)),
                        time_ns,
                        int(round(float(safe_value(arrays, mapping["trackID"], i, 0.0)))),
                        int(round(float(safe_value(arrays, mapping["eventID"], i, 0.0)))),
                    ]

                    if all(math.isfinite(float(x)) for x in row[:9]):
                        writer.writerow(row)
                        written += 1

        print(f"\nWrote {written} rows.")


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Convert ROOT stepping/track data into BeamLabStudio Geant4-compatible CSV."
    )

    parser.add_argument("input", help="Input .root file")
    parser.add_argument("--output", "-o", default="examples/datasets/geant4_csv/from_root.csv")
    parser.add_argument("--tree", default=None, help="TTree name. If omitted, first tree-like object is used.")
    parser.add_argument("--list", action="store_true", help="List ROOT objects and branches, then exit.")
    parser.add_argument("--position-unit", default="cm", choices=["cm", "mm", "m"])
    parser.add_argument("--time-unit", default="ns", choices=["ns", "us", "ms", "s"])
    parser.add_argument("--step-size", default="100 MB")
    parser.add_argument("--max-rows", type=int, default=None)

    args = parser.parse_args()

    if args.list:
        list_root_file(Path(args.input).expanduser().resolve(), args.tree)
        return

    convert_root_to_csv(args)


if __name__ == "__main__":
    main()
