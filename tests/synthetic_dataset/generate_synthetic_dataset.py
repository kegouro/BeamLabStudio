#!/usr/bin/env python3
"""
BeamLabStudio Synthetic Test Dataset Generator
===============================================
Generates six self-contained test runs covering normal operation and all
known edge cases that can crash or silently mis-render the application.

Usage:
    python3 generate_synthetic_dataset.py [--outdir PATH]

Default output: tests/synthetic_dataset/
Each test case produces a fully self-contained run directory that can be
opened directly with File → Open in BeamLabStudio.

Author: BeamLabStudio test suite
"""

import argparse
import csv
import json
import math
import os
import random
import shutil
import sys
from pathlib import Path


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def makedirs(*parts):
    p = Path(*parts)
    p.mkdir(parents=True, exist_ok=True)
    return p


def write_csv(path, header, rows):
    with open(path, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(header)
        w.writerows(rows)


def write_obj_polylines(path, polylines, comment=""):
    """Write a Wavefront OBJ with 'l' elements (one per polyline).

    polylines: list of lists of (x, y, z) tuples [meters]
    """
    with open(path, "w") as f:
        if comment:
            f.write(f"# {comment}\n")
        f.write("# vertices: x_m y_m z_m\n")
        vertex_index = 1
        line_groups = []
        for poly in polylines:
            start = vertex_index
            for (x, y, z) in poly:
                f.write(f"v {x:.6e} {y:.6e} {z:.6e}\n")
                vertex_index += 1
            end = vertex_index - 1
            line_groups.append((start, end))
        f.write("\n")
        for (start, end) in line_groups:
            indices = " ".join(str(i) for i in range(start, end + 1))
            f.write(f"l {indices}\n")


def write_obj_mesh(path, vertices, faces, comment=""):
    """Write a simple triangulated mesh OBJ (for caustic / lens objects)."""
    with open(path, "w") as f:
        if comment:
            f.write(f"# {comment}\n")
        for (x, y, z) in vertices:
            f.write(f"v {x:.6e} {y:.6e} {z:.6e}\n")
        f.write("\n")
        for tri in faces:
            f.write("f {} {} {}\n".format(*[i + 1 for i in tri]))


def write_manifest(path, run_dir, **extras):
    """Write a manifest JSON for the given run_dir layout.

    The manifest lives at  run_dir/visualization/manifest.json
    All CSV/OBJ paths are stored relative to the visualization/ directory.
    """
    vis = Path(run_dir) / "visualization"
    data = {
        "axis_mode": extras.get("axis_mode", "z"),
        "reference_mode": extras.get("reference_mode", "axial"),
        "binning_mode": extras.get("binning_mode", "uniform"),
        "axial_bin_count": extras.get("axial_bin_count", 20),
        "half_window_frames": extras.get("half_window_frames", 5),
        "trajectories_preview": extras.get("trajectories_preview", 50),
        "focal_slice_points": extras.get("focal_slice_points", 200),
        "envelope_rings": extras.get("envelope_rings", 20),
        "trajectories_preview_csv": "trajectories_preview.csv",
        "focal_slice_points_csv": "focal_slice_points.csv",
        "envelope_rings_csv": "envelope_rings.csv",
        "trajectories_preview_obj": "trajectories_preview.obj",
        "beam_caustic_preview_obj": "beam_caustic_preview.obj",
        "effective_lens_body_preview_obj": "effective_lens_body_preview.obj",
        "full_beam_envelope_preview_obj": extras.get("full_envelope_obj", ""),
    }
    # Overlay any caller-supplied overrides (e.g. missing paths)
    data.update({k: v for k, v in extras.items()
                 if k not in ("axis_mode", "reference_mode", "binning_mode",
                              "axial_bin_count", "half_window_frames",
                              "trajectories_preview", "focal_slice_points",
                              "envelope_rings", "full_envelope_obj")})

    with open(path, "w") as f:
        json.dump(data, f, indent=2)


# ---------------------------------------------------------------------------
# Physics helpers
# ---------------------------------------------------------------------------

def gaussian_beam_radius(z_m, z_focus_m, w0_m, z_rayleigh_m):
    """1/e² Gaussian beam half-width at axial position z."""
    dz = z_m - z_focus_m
    return w0_m * math.sqrt(1.0 + (dz / z_rayleigh_m) ** 2)


def generate_beam_trajectory(n_steps, z_start, z_end, rng,
                              z_focus, w0, z_r, kinE_start=150.0,
                              sigma_transverse=0.0):
    """Return list of (step_index, z_m, x_m, y_m, kinE_MeV) for one trajectory."""
    pts = []
    phi = rng.uniform(0, 2 * math.pi)   # launch angle
    for s in range(n_steps):
        z = z_start + (z_end - z_start) * s / (n_steps - 1)
        r = gaussian_beam_radius(z, z_focus, w0, z_r)
        # Gaussian transverse distribution at this z
        r_sample = rng.gauss(0.0, r)
        x = r_sample * math.cos(phi) + rng.gauss(0, sigma_transverse)
        y = r_sample * math.sin(phi) + rng.gauss(0, sigma_transverse)
        # Simple linear energy loss (real Bragg would peak at end)
        kinE = max(0.001, kinE_start * (1.0 - s / n_steps))
        pts.append((s, z, x, y, kinE))
    return pts


def generate_bragg_trajectory(event_id, track_id, rng,
                               z_start=0.0, z_end=0.15,
                               n_steps=30, kinE0=150.0,
                               transverse_sigma=0.003):
    """Return rows for energy_step_profile.csv for one proton track
    with a realistic Bragg peak energy-loss profile."""
    rows = []
    x0 = rng.gauss(0, transverse_sigma)
    y0 = rng.gauss(0, transverse_sigma)
    kinE = kinE0
    for s in range(n_steps):
        frac = s / (n_steps - 1)
        z = z_start + (z_end - z_start) * frac
        # Bragg-like dE/dx: rises steeply near end (frac → 1)
        dEdx = 0.5 + 8.0 * frac ** 4   # MeV/cm equivalent
        step_len_cm = (z_end - z_start) * 100.0 / (n_steps - 1)
        edep = min(kinE, dEdx * step_len_cm * rng.uniform(0.85, 1.15))
        edep = max(0.0, edep)
        kinE_new = max(0.0, kinE - edep)

        rows.append([
            event_id, track_id, s,
            z,                         # axial_m
            edep,                      # edep_MeV
            edep * 1e6,                # edep_eV
            kinE,                      # kinE_MeV (before this step)
            edep * 1.6e-10 / (1e-3),   # dose_Gy (rough, mass ~ 1g)
            x0 * (1.0 - frac * 0.2),   # x_m (slight focusing)
            y0 * (1.0 - frac * 0.2),   # y_m
            z,                         # z_m (same as axial for on-axis beam)
        ])
        kinE = kinE_new
    return rows


# ---------------------------------------------------------------------------
# OBJ geometry helpers
# ---------------------------------------------------------------------------

def make_ring_mesh(z_center, inner_r, outer_r, n_segments=32):
    """Annular ring mesh (two concentric circles + quad strip)."""
    verts = []
    faces = []
    for i in range(n_segments):
        angle = 2 * math.pi * i / n_segments
        cos_a, sin_a = math.cos(angle), math.sin(angle)
        verts.append((cos_a * outer_r, sin_a * outer_r, z_center))
        verts.append((cos_a * inner_r, sin_a * inner_r, z_center))
    for i in range(n_segments):
        a = i * 2
        b = ((i + 1) % n_segments) * 2
        faces.append((a, b, b + 1))
        faces.append((a, b + 1, a + 1))
    return verts, faces


def make_lens_mesh(z_start, z_end, radius, n_segments=16):
    """Simple cylinder for effective lens preview."""
    verts = []
    faces = []
    for iz, z in enumerate([z_start, z_end]):
        for i in range(n_segments):
            angle = 2 * math.pi * i / n_segments
            verts.append((radius * math.cos(angle), radius * math.sin(angle), z))
    for i in range(n_segments):
        a = i
        b = (i + 1) % n_segments
        c = b + n_segments
        d = a + n_segments
        faces.append((a, b, c))
        faces.append((a, c, d))
    return verts, faces


# ---------------------------------------------------------------------------
# Test case generators
# ---------------------------------------------------------------------------

def tc01_normal_beam(base_dir):
    """
    TC-01: Normal converging beam.

    Geometry: 50 trajectories, z ∈ [0, 0.30] m, focus at z=0.15 m.
    Expected: focus_curve has a clear minimum at z≈0.15 m.
             RMS radius at focus ≈ 0.50 mm.
             20 envelope slices, all valid.
             BioSim: 50 tracks, 30 steps each → 1500 rows total.
    """
    rng = random.Random(42)
    run = makedirs(base_dir, "TC01_normal_beam")
    vis = makedirs(run, "visualization")
    tables = makedirs(run, "tables")

    N_TRAJ = 50
    N_STEPS = 40
    Z_FOCUS = 0.15
    W0 = 0.0005       # 0.5 mm waist radius
    Z_R = 0.04        # 4 cm Rayleigh range
    Z_START, Z_END = 0.0, 0.30

    # ── Trajectories CSV ──────────────────────────────────────────────────
    traj_rows = []
    for t in range(N_TRAJ):
        pts = generate_beam_trajectory(N_STEPS, Z_START, Z_END, rng,
                                       Z_FOCUS, W0, Z_R, kinE_start=150.0)
        for (s, z, x, y, kinE) in pts:
            traj_rows.append([t, s, z, x, y, kinE])
    write_csv(vis / "trajectories_preview.csv",
              ["trajectory_index", "step_index", "z_m", "x_m", "y_m", "kinE_MeV"],
              traj_rows)

    # ── Trajectories OBJ ─────────────────────────────────────────────────
    polylines = []
    for t in range(N_TRAJ):
        pts = [(row[2], row[3], row[4])   # z, x, y → reorder to x,y,z for OBJ
               for row in traj_rows if row[0] == t]
        polylines.append([(x, y, z) for (z, x, y) in pts])
    write_obj_polylines(vis / "trajectories_preview.obj", polylines,
                        "TC01 normal beam trajectories")

    # ── Focal slice CSV ───────────────────────────────────────────────────
    focal_rows = []
    for _ in range(200):
        r = abs(rng.gauss(0, W0))
        phi = rng.uniform(0, 2 * math.pi)
        focal_rows.append([r * math.cos(phi), r * math.sin(phi)])
    write_csv(vis / "focal_slice_points.csv", ["u_m", "v_m"], focal_rows)

    # ── Envelope rings CSV ────────────────────────────────────────────────
    N_SLICES = 20
    env_rows = []
    z_slices = [Z_START + (Z_END - Z_START) * i / (N_SLICES - 1)
                for i in range(N_SLICES)]
    for si, z in enumerate(z_slices):
        r = gaussian_beam_radius(z, Z_FOCUS, W0, Z_R)
        n_pts = 32
        for k in range(n_pts):
            ang = 2 * math.pi * k / n_pts
            env_rows.append([si, z,
                              r * math.cos(ang),
                              r * math.sin(ang)])
    write_csv(vis / "envelope_rings.csv",
              ["slice_index", "z_m", "u_m", "v_m"], env_rows)

    # ── Caustic OBJ (annular rings at each slice) ─────────────────────────
    all_verts, all_faces = [], []
    offset = 0
    for z in z_slices[::2]:
        r = gaussian_beam_radius(z, Z_FOCUS, W0, Z_R)
        v, f = make_ring_mesh(z, r * 0.8, r)
        all_faces += [(a + offset, b + offset, c + offset) for (a, b, c) in f]
        all_verts += v
        offset += len(v)
    write_obj_mesh(vis / "beam_caustic_preview.obj", all_verts, all_faces,
                   "TC01 focal envelope proxy")

    # ── Lens OBJ ──────────────────────────────────────────────────────────
    lv, lf = make_lens_mesh(Z_FOCUS - 0.01, Z_FOCUS + 0.01, W0 * 1.5)
    write_obj_mesh(vis / "effective_lens_body_preview.obj", lv, lf,
                   "TC01 effective lens")

    # ── focus_curve.csv ───────────────────────────────────────────────────
    fc_rows = []
    for si, z in enumerate(z_slices):
        r_rms = gaussian_beam_radius(z, Z_FOCUS, W0, Z_R) / math.sqrt(2)
        is_focus = (si == N_SLICES // 2)
        fc_rows.append([si, z, r_rms, N_TRAJ, str(is_focus).lower()])
    write_csv(tables / "focus_curve.csv",
              ["index", "reference_value_m", "metric_value_m",
               "point_count", "is_focus"],
              fc_rows)

    # ── envelope_summary.csv ──────────────────────────────────────────────
    es_rows = []
    for si, z in enumerate(z_slices):
        r = gaussian_beam_radius(z, Z_FOCUS, W0, Z_R)
        area = math.pi * r ** 2
        perim = 2 * math.pi * r
        es_rows.append([si, z, z, N_TRAJ, max(4, int(N_TRAJ * 0.7)),
                        f"{area:.6e}", f"{perim:.6e}",
                        "ConvexHull", "true"])
    write_csv(tables / "envelope_summary.csv",
              ["slice_index", "reference_value_m", "axial_position_m",
               "input_points", "boundary_points",
               "area_m2", "perimeter_m", "method_name", "valid"],
              es_rows)

    # ── BioSim energy_step_profile.csv ────────────────────────────────────
    bio_rows = []
    for ev in range(N_TRAJ):
        bio_rows.extend(generate_bragg_trajectory(ev, 0, rng, n_steps=30))
    write_csv(run / "energy_step_profile.csv",
              ["event_id", "track_id", "step_index", "axial_m",
               "edep_MeV", "edep_eV", "kinE_MeV", "dose_Gy",
               "x_m", "y_m", "z_m"],
              bio_rows)

    # ── Manifest ──────────────────────────────────────────────────────────
    write_manifest(vis / "manifest.json", run,
                   trajectories_preview=N_TRAJ,
                   focal_slice_points=200,
                   envelope_rings=N_SLICES,
                   axial_bin_count=N_SLICES)

    return run


def tc02_tight_focus(base_dir):
    """
    TC-02: Very tight focus (sub-mm waist, high Rayleigh ratio).

    Tests: color scale bar at extreme gradient stretch.
    Expected: RMS radius at focus ≈ 0.05 mm.
             Area ratio (max/min) > 100×.
             No crash in scale bar rendering.
    """
    rng = random.Random(7)
    run = makedirs(base_dir, "TC02_tight_focus")
    vis = makedirs(run, "visualization")
    tables = makedirs(run, "tables")

    N_TRAJ = 30
    N_STEPS = 50
    Z_FOCUS = 0.10
    W0 = 0.00005       # 50 µm waist
    Z_R = 0.005        # 5 mm Rayleigh range — very tight
    Z_START, Z_END = 0.0, 0.20
    N_SLICES = 25

    traj_rows = []
    for t in range(N_TRAJ):
        pts = generate_beam_trajectory(N_STEPS, Z_START, Z_END, rng,
                                       Z_FOCUS, W0, Z_R, kinE_start=80.0)
        for (s, z, x, y, kinE) in pts:
            traj_rows.append([t, s, z, x, y, kinE])
    write_csv(vis / "trajectories_preview.csv",
              ["trajectory_index", "step_index", "z_m", "x_m", "y_m", "kinE_MeV"],
              traj_rows)

    polylines = []
    for t in range(N_TRAJ):
        pts = [(row[2], row[3], row[4]) for row in traj_rows if row[0] == t]
        polylines.append([(x, y, z) for (z, x, y) in pts])
    write_obj_polylines(vis / "trajectories_preview.obj", polylines,
                        "TC02 tight-focus beam")

    focal_rows = [[rng.gauss(0, W0), rng.gauss(0, W0)] for _ in range(300)]
    write_csv(vis / "focal_slice_points.csv", ["u_m", "v_m"], focal_rows)

    z_slices = [Z_START + (Z_END - Z_START) * i / (N_SLICES - 1)
                for i in range(N_SLICES)]
    env_rows = []
    for si, z in enumerate(z_slices):
        r = gaussian_beam_radius(z, Z_FOCUS, W0, Z_R)
        for k in range(32):
            ang = 2 * math.pi * k / 32
            env_rows.append([si, z, r * math.cos(ang), r * math.sin(ang)])
    write_csv(vis / "envelope_rings.csv",
              ["slice_index", "z_m", "u_m", "v_m"], env_rows)

    all_verts, all_faces, offset = [], [], 0
    for z in z_slices[::3]:
        r = gaussian_beam_radius(z, Z_FOCUS, W0, Z_R)
        v, f = make_ring_mesh(z, r * 0.8, r)
        all_faces += [(a + offset, b + offset, c + offset) for a, b, c in f]
        all_verts += v
        offset += len(v)
    write_obj_mesh(vis / "beam_caustic_preview.obj", all_verts, all_faces)
    lv, lf = make_lens_mesh(Z_FOCUS - 0.005, Z_FOCUS + 0.005, W0 * 2)
    write_obj_mesh(vis / "effective_lens_body_preview.obj", lv, lf)

    fc_rows = []
    for si, z in enumerate(z_slices):
        r_rms = gaussian_beam_radius(z, Z_FOCUS, W0, Z_R) / math.sqrt(2)
        fc_rows.append([si, z, r_rms, N_TRAJ,
                        str(si == N_SLICES // 2).lower()])
    write_csv(tables / "focus_curve.csv",
              ["index", "reference_value_m", "metric_value_m",
               "point_count", "is_focus"],
              fc_rows)

    es_rows = []
    for si, z in enumerate(z_slices):
        r = gaussian_beam_radius(z, Z_FOCUS, W0, Z_R)
        es_rows.append([si, z, z, N_TRAJ, max(3, int(N_TRAJ * 0.6)),
                        f"{math.pi * r**2:.6e}", f"{2*math.pi*r:.6e}",
                        "ConvexHull", "true"])
    write_csv(tables / "envelope_summary.csv",
              ["slice_index", "reference_value_m", "axial_position_m",
               "input_points", "boundary_points",
               "area_m2", "perimeter_m", "method_name", "valid"],
              es_rows)

    bio_rows = []
    for ev in range(N_TRAJ):
        bio_rows.extend(generate_bragg_trajectory(ev, 0, rng,
                        z_end=0.20, n_steps=50, kinE0=80.0,
                        transverse_sigma=0.0001))
    write_csv(run / "energy_step_profile.csv",
              ["event_id", "track_id", "step_index", "axial_m",
               "edep_MeV", "edep_eV", "kinE_MeV", "dose_Gy",
               "x_m", "y_m", "z_m"],
              bio_rows)

    write_manifest(vis / "manifest.json", run,
                   trajectories_preview=N_TRAJ,
                   focal_slice_points=300,
                   envelope_rings=N_SLICES)
    return run


def tc03_no_focus(base_dir):
    """
    TC-03: Monotonically diverging beam — no focus detected.

    Tests: is_focus=false for all rows, beam_radius_focus_row_ stays -1,
           BeamRadiusInspector shows "No radius profile loaded" fallback.
    Expected: focus_curve has no minimum (all radii increase),
             inspector offset spinner stays disabled,
             chart renders with "No data available" note label
             (since is_focus never true — no highlight_minimum dot).
    """
    rng = random.Random(99)
    run = makedirs(base_dir, "TC03_no_focus")
    vis = makedirs(run, "visualization")
    tables = makedirs(run, "tables")

    N_TRAJ = 20
    N_STEPS = 30
    N_SLICES = 15
    Z_START, Z_END = 0.0, 0.25

    traj_rows = []
    for t in range(N_TRAJ):
        for s in range(N_STEPS):
            z = Z_START + (Z_END - Z_START) * s / (N_STEPS - 1)
            r = 0.001 + 0.015 * (z / Z_END)   # linearly diverging
            phi = rng.uniform(0, 2 * math.pi)
            x = r * math.cos(phi)
            y = r * math.sin(phi)
            kinE = max(0.01, 150.0 * (1.0 - s / N_STEPS))
            traj_rows.append([t, s, z, x, y, kinE])
    write_csv(vis / "trajectories_preview.csv",
              ["trajectory_index", "step_index", "z_m", "x_m", "y_m", "kinE_MeV"],
              traj_rows)

    polylines = []
    for t in range(N_TRAJ):
        pts = [(row[2], row[3], row[4]) for row in traj_rows if row[0] == t]
        polylines.append([(x, y, z) for (z, x, y) in pts])
    write_obj_polylines(vis / "trajectories_preview.obj", polylines,
                        "TC03 diverging beam — no focus")

    # focal slice at end (large spread)
    R_MAX = 0.015
    focal_rows = []
    for _ in range(150):
        r = abs(rng.gauss(0, R_MAX))
        phi = rng.uniform(0, 2 * math.pi)
        focal_rows.append([r * math.cos(phi), r * math.sin(phi)])
    write_csv(vis / "focal_slice_points.csv", ["u_m", "v_m"], focal_rows)

    z_slices = [Z_START + (Z_END - Z_START) * i / (N_SLICES - 1)
                for i in range(N_SLICES)]
    env_rows = []
    for si, z in enumerate(z_slices):
        r = 0.001 + 0.015 * z / Z_END
        for k in range(24):
            ang = 2 * math.pi * k / 24
            env_rows.append([si, z, r * math.cos(ang), r * math.sin(ang)])
    write_csv(vis / "envelope_rings.csv",
              ["slice_index", "z_m", "u_m", "v_m"], env_rows)

    all_verts, all_faces, offset = [], [], 0
    for z in z_slices:
        r = 0.001 + 0.015 * z / Z_END
        v, f = make_ring_mesh(z, r * 0.8, r, n_segments=20)
        all_faces += [(a + offset, b + offset, c + offset) for a, b, c in f]
        all_verts += v
        offset += len(v)
    write_obj_mesh(vis / "beam_caustic_preview.obj", all_verts, all_faces)
    lv, lf = make_lens_mesh(0.24, 0.25, 0.005)
    write_obj_mesh(vis / "effective_lens_body_preview.obj", lv, lf)

    fc_rows = []
    for si, z in enumerate(z_slices):
        r_rms = (0.001 + 0.015 * z / Z_END) / math.sqrt(2)
        fc_rows.append([si, z, r_rms, N_TRAJ, "false"])  # no focus ever
    write_csv(tables / "focus_curve.csv",
              ["index", "reference_value_m", "metric_value_m",
               "point_count", "is_focus"],
              fc_rows)

    es_rows = []
    for si, z in enumerate(z_slices):
        r = 0.001 + 0.015 * z / Z_END
        es_rows.append([si, z, z, N_TRAJ, max(3, int(N_TRAJ * 0.65)),
                        f"{math.pi * r**2:.6e}", f"{2*math.pi*r:.6e}",
                        "AngularSectors", "true"])
    write_csv(tables / "envelope_summary.csv",
              ["slice_index", "reference_value_m", "axial_position_m",
               "input_points", "boundary_points",
               "area_m2", "perimeter_m", "method_name", "valid"],
              es_rows)

    bio_rows = []
    for ev in range(N_TRAJ):
        bio_rows.extend(generate_bragg_trajectory(ev, 0, rng,
                        z_end=0.25, n_steps=30, kinE0=150.0,
                        transverse_sigma=0.01))
    write_csv(run / "energy_step_profile.csv",
              ["event_id", "track_id", "step_index", "axial_m",
               "edep_MeV", "edep_eV", "kinE_MeV", "dose_Gy",
               "x_m", "y_m", "z_m"],
              bio_rows)

    write_manifest(vis / "manifest.json", run,
                   trajectories_preview=N_TRAJ,
                   focal_slice_points=150,
                   envelope_rings=N_SLICES)
    return run


def tc04_edge_single_trajectory(base_dir):
    """
    TC-04: Edge — single trajectory, minimum 2 steps.

    Tests: polylineCount()==1, count slider range [0,1],
           energy gradient with 2 vertices (min==max → scale forced to [0,1]),
           focus_curve with one row — inspect spinner range must be [0,0].
    Expected: no crash, OBJ viewer shows one line segment,
             gradient falls back to 0.5 (single-value clamp),
             inspector shows the single row correctly.
    """
    rng = random.Random(13)
    run = makedirs(base_dir, "TC04_edge_single_trajectory")
    vis = makedirs(run, "visualization")
    tables = makedirs(run, "tables")

    # Two-step trajectory (minimum to form a polyline)
    traj_rows = [
        [0, 0, 0.0,  0.001,  0.001, 150.0],
        [0, 1, 0.15, 0.0005, 0.0005, 120.0],
    ]
    write_csv(vis / "trajectories_preview.csv",
              ["trajectory_index", "step_index", "z_m", "x_m", "y_m", "kinE_MeV"],
              traj_rows)

    write_obj_polylines(vis / "trajectories_preview.obj",
                        [[(0.001, 0.001, 0.0), (0.0005, 0.0005, 0.15)]],
                        "TC04 single two-step trajectory")

    write_csv(vis / "focal_slice_points.csv", ["u_m", "v_m"],
              [[0.0005, 0.0005]])  # single focal point

    write_csv(vis / "envelope_rings.csv", ["slice_index", "z_m", "u_m", "v_m"],
              [[0, 0.15, 0.0005, 0.0005]])   # one point, one ring

    # Minimal caustic and lens
    write_obj_mesh(vis / "beam_caustic_preview.obj",
                   [(0, 0, 0.15)], [])
    write_obj_mesh(vis / "effective_lens_body_preview.obj",
                   [(0, 0, 0.15)], [])

    write_csv(tables / "focus_curve.csv",
              ["index", "reference_value_m", "metric_value_m",
               "point_count", "is_focus"],
              [[0, 0.15, 0.0003535, 1, "true"]])

    write_csv(tables / "envelope_summary.csv",
              ["slice_index", "reference_value_m", "axial_position_m",
               "input_points", "boundary_points",
               "area_m2", "perimeter_m", "method_name", "valid"],
              [[0, 0.15, 0.15, 1, 1,
                "7.85e-8", "9.95e-4", "ConvexHull", "true"]])

    # BioSim: single track, single step (absolute minimum)
    write_csv(run / "energy_step_profile.csv",
              ["event_id", "track_id", "step_index", "axial_m",
               "edep_MeV", "edep_eV", "kinE_MeV", "dose_Gy",
               "x_m", "y_m", "z_m"],
              [[0, 0, 0, 0.075, 0.5, 5e5, 149.5, 8e-8, 0.001, 0.001, 0.075]])

    write_manifest(vis / "manifest.json", run,
                   trajectories_preview=1,
                   focal_slice_points=1,
                   envelope_rings=1)
    return run


def tc05_edge_biosim_many_tracks(base_dir):
    """
    TC-05: BioSim stress test — 1200 proton tracks × 30 steps = 36 000 rows.

    Tests: the dangling-pointer crash fix in BioSimRunner::loadCsv.
           With the old code this ALWAYS crashed during vector reallocation.
           With the fix (std::map<key, BioTrack>) it must complete without error.

    Expected: BioSim reports 1200 tracks, ~36 000 steps,
             kinE range ≈ [0, 150] MeV,
             no crash, no assertion failure.
    Physics: 150 MeV protons in water, 30-step Bragg profile.
    """
    rng = random.Random(2025)
    run = makedirs(base_dir, "TC05_biosim_stress")
    vis = makedirs(run, "visualization")
    tables = makedirs(run, "tables")

    N_TRAJ = 1200
    N_STEPS = 30

    # Re-use TC01 beam geometry for the visualization files
    Z_FOCUS, W0, Z_R = 0.15, 0.0005, 0.04
    Z_START, Z_END = 0.0, 0.30
    N_SLICES = 20

    traj_rows = []
    # Only export 50 representative trajectories for visualisation
    for t in range(50):
        pts = generate_beam_trajectory(N_STEPS, Z_START, Z_END, rng,
                                       Z_FOCUS, W0, Z_R, kinE_start=150.0)
        for (s, z, x, y, kinE) in pts:
            traj_rows.append([t, s, z, x, y, kinE])
    write_csv(vis / "trajectories_preview.csv",
              ["trajectory_index", "step_index", "z_m", "x_m", "y_m", "kinE_MeV"],
              traj_rows)

    polylines = []
    for t in range(50):
        pts = [(row[2], row[3], row[4]) for row in traj_rows if row[0] == t]
        polylines.append([(x, y, z) for (z, x, y) in pts])
    write_obj_polylines(vis / "trajectories_preview.obj", polylines,
                        "TC05 stress test (50 of 1200 shown)")

    focal_rows = [[rng.gauss(0, W0), rng.gauss(0, W0)] for _ in range(200)]
    write_csv(vis / "focal_slice_points.csv", ["u_m", "v_m"], focal_rows)

    z_slices = [Z_START + (Z_END - Z_START) * i / (N_SLICES - 1)
                for i in range(N_SLICES)]
    env_rows = []
    for si, z in enumerate(z_slices):
        r = gaussian_beam_radius(z, Z_FOCUS, W0, Z_R)
        for k in range(32):
            ang = 2 * math.pi * k / 32
            env_rows.append([si, z, r * math.cos(ang), r * math.sin(ang)])
    write_csv(vis / "envelope_rings.csv",
              ["slice_index", "z_m", "u_m", "v_m"], env_rows)

    all_verts, all_faces, offset = [], [], 0
    for z in z_slices[::2]:
        r = gaussian_beam_radius(z, Z_FOCUS, W0, Z_R)
        v, f = make_ring_mesh(z, r * 0.8, r)
        all_faces += [(a + offset, b + offset, c + offset) for a, b, c in f]
        all_verts += v
        offset += len(v)
    write_obj_mesh(vis / "beam_caustic_preview.obj", all_verts, all_faces)
    lv, lf = make_lens_mesh(Z_FOCUS - 0.01, Z_FOCUS + 0.01, W0 * 1.5)
    write_obj_mesh(vis / "effective_lens_body_preview.obj", lv, lf)

    fc_rows = []
    for si, z in enumerate(z_slices):
        r_rms = gaussian_beam_radius(z, Z_FOCUS, W0, Z_R) / math.sqrt(2)
        fc_rows.append([si, z, r_rms, N_TRAJ, str(si == N_SLICES // 2).lower()])
    write_csv(tables / "focus_curve.csv",
              ["index", "reference_value_m", "metric_value_m",
               "point_count", "is_focus"],
              fc_rows)

    es_rows = []
    for si, z in enumerate(z_slices):
        r = gaussian_beam_radius(z, Z_FOCUS, W0, Z_R)
        es_rows.append([si, z, z, N_TRAJ, max(4, int(N_TRAJ * 0.7)),
                        f"{math.pi * r**2:.6e}", f"{2*math.pi*r:.6e}",
                        "ConvexHull", "true"])
    write_csv(tables / "envelope_summary.csv",
              ["slice_index", "reference_value_m", "axial_position_m",
               "input_points", "boundary_points",
               "area_m2", "perimeter_m", "method_name", "valid"],
              es_rows)

    # 1200 tracks × 30 steps — THIS is the crash-regression dataset
    print(f"  Generating {N_TRAJ} tracks × {N_STEPS} steps "
          f"({N_TRAJ * N_STEPS} rows) for BioSim …", end=" ", flush=True)
    bio_rows = []
    for ev in range(N_TRAJ):
        bio_rows.extend(generate_bragg_trajectory(ev, 0, rng, n_steps=N_STEPS,
                        transverse_sigma=rng.uniform(0.001, 0.005)))
    write_csv(run / "energy_step_profile.csv",
              ["event_id", "track_id", "step_index", "axial_m",
               "edep_MeV", "edep_eV", "kinE_MeV", "dose_Gy",
               "x_m", "y_m", "z_m"],
              bio_rows)
    print("done")

    write_manifest(vis / "manifest.json", run,
                   trajectories_preview=50,
                   focal_slice_points=200,
                   envelope_rings=N_SLICES)
    return run


def tc06_edge_missing_files(base_dir):
    """
    TC-06: Graceful degradation — manifest points to non-existent files.

    Tests: every file-missing branch in loadManifest and
           StatsDashboardWidget::loadRunFromManifest.
    Expected: app loads without crash, shows "No data available" /
             "Missing file:" messages in the dashboard and chart areas,
             status bar shows "Loaded: <path>".
    """
    rng = random.Random(55)
    run = makedirs(base_dir, "TC06_missing_files")
    vis = makedirs(run, "visualization")
    tables = makedirs(run, "tables")

    # Only the manifest and one OBJ are present — everything else is missing.
    # The one OBJ has a single two-point polyline so the 3D viewer shows something.
    write_obj_polylines(vis / "trajectories_preview.obj",
                        [[(0, 0, 0), (0, 0, 0.3)]],
                        "TC06 stub OBJ — other files intentionally missing")

    # An empty but valid focus_curve so the stats tab doesn't divide by zero.
    write_csv(tables / "focus_curve.csv",
              ["index", "reference_value_m", "metric_value_m",
               "point_count", "is_focus"],
              [])

    write_csv(tables / "envelope_summary.csv",
              ["slice_index", "reference_value_m", "axial_position_m",
               "input_points", "boundary_points",
               "area_m2", "perimeter_m", "method_name", "valid"],
              [])

    # Manifest deliberately references missing files for CSV paths
    data = {
        "axis_mode": "z",
        "reference_mode": "axial",
        "binning_mode": "uniform",
        "axial_bin_count": 10,
        "half_window_frames": 3,
        "trajectories_preview": 0,
        "focal_slice_points": 0,
        "envelope_rings": 0,
        "trajectories_preview_csv": "MISSING_trajectories.csv",
        "focal_slice_points_csv": "MISSING_focal_slice.csv",
        "envelope_rings_csv": "MISSING_envelope.csv",
        "trajectories_preview_obj": "trajectories_preview.obj",   # exists
        "beam_caustic_preview_obj": "MISSING_caustic.obj",
        "effective_lens_body_preview_obj": "MISSING_lens.obj",
        "full_beam_envelope_preview_obj": "",
    }
    with open(vis / "manifest.json", "w") as f:
        json.dump(data, f, indent=2)

    # BioSim CSV is also absent — setCsvPath is NOT called (file doesn't exist)
    # so BioSim should show "Ready" and the Run button simply warns "No file".

    return run


# ---------------------------------------------------------------------------
# Expected results report
# ---------------------------------------------------------------------------

EXPECTED_RESULTS_MD = """\
# BeamLabStudio Synthetic Dataset — Expected Results

Generated by `generate_synthetic_dataset.py`.

---

## How to run

1. Build the application: `cmake --build build-ui --target beamlab_ui`
2. Launch: `./build-ui/beamlab_ui`
3. Open each manifest via **File → Open** (or drag-and-drop onto the window).
4. Compare what you see against the expectations below.

Each run directory contains a self-contained `visualization/manifest.json`
that can be opened directly.

---

## TC-01 · Normal converging beam

**File:** `TC01_normal_beam/visualization/manifest.json`

| What to check | Expected value |
|---|---|
| Dashboard loads without error | ✓ |
| Beam RMS radius chart | Clear U-shape; minimum at z ≈ 0.150 m |
| RMS radius at focus | ≈ 0.354 mm (= W₀/√2 = 0.5/√2 mm) |
| RMS radius at edges (z=0 and z=0.30 m) | ≈ 2.86 mm (= √(1+(0.15/0.04)²)×0.354 mm) |
| Area curve | Minimum ≈ 7.85×10⁻⁷ m² at z=0.15 m |
| Area at z=0 and z=0.30 m | ≈ 2.57×10⁻⁴ m² (≈327× the minimum) |
| Points per slice | Constant at 50 across all slices |
| Radial histogram | Roughly Rayleigh distribution, peak < 1 mm |
| Beam radius inspector | Focus row highlighted (is_focus=true at index 10) |
| Min radius offset spin | Range: −10 to +9 |
| Individual trajectories 3D | 50 coloured polylines converging then diverging |
| Energy gradient | Gradient from blue (low kinE) to red (high kinE) |
| Scale bar | Shows [0, 150] MeV range |
| Click-to-pick (3D viewer) | Shows energy value in label |
| BioSim (CSV auto-loaded) | Path auto-filled in toolbar label |
| BioSim Run | Completes without crash; 50 tracks, 30 steps each |
| BioSim viewport | 50 coloured Bragg trajectories visible |
| Focal envelope rings OBJ | Smooth ring stack (128 segments with TC build) |

---

## TC-02 · Tight focus (sub-mm waist)

**File:** `TC02_tight_focus/visualization/manifest.json`

| What to check | Expected value |
|---|---|
| RMS radius at focus | ≈ 0.035 mm (= 50 µm / √2) |
| Area ratio max/min | > 100× |
| Scale bar on energy gradient | Must render without crash at extreme scale |
| BioSim | 30 tracks, 50 steps each; kinE₀ = 80 MeV |
| BioSim Bragg peak | Visible concentration of colour at z ≈ 0.20 m |

---

## TC-03 · No focus (diverging beam)

**File:** `TC03_no_focus/visualization/manifest.json`

| What to check | Expected value |
|---|---|
| Focus curve chart | Monotonically increasing — no U-shape |
| Beam radius inspector label | "No radius profile loaded" (focus_row = −1) |
| Inspector offset spin | Disabled (no focus row found) |
| Area curve | Monotonically increasing |
| RMS at z=0.25 m | ≈ 10.6 mm |
| BioSim | 20 tracks, each with large transverse spread |

---

## TC-04 · Edge: single two-step trajectory

**File:** `TC04_edge_single_trajectory/visualization/manifest.json`

| What to check | Expected value |
|---|---|
| OBJ viewer (Individual 3D) | One line segment visible |
| Trajectory count slider | Range [0, 1]; value = 1 |
| Energy gradient on single segment | No crash; gradient from blue→red (2 values) |
| Scale bar | kinE range [120, 150] MeV |
| Focus curve | One row: z=0.15, r_rms=0.354 mm, is_focus=true |
| Inspector spin | Range [0, 0]; shows that single row |
| Focal slice | One dot at the origin; histogram shows 1 bin |
| BioSim single step | 1 track, 1 step; completes immediately |
| BioSim viewport | Single dot visible in the 3D view |

---

## TC-05 · BioSim stress test (1200 tracks)

**File:** `TC05_biosim_stress/visualization/manifest.json`

| What to check | Expected value |
|---|---|
| Visualization tab loads | Normal (shows 50 preview trajectories) |
| BioSim CSV auto-loaded | Yes — `energy_step_profile.csv` in run dir |
| BioSim Run | **Must complete without crash** (this was the dangling-pointer bug) |
| Total tracks reported | 1200 |
| Total steps reported | ≈ 36 000 |
| kinE range | [0, 150] MeV |
| Viewport performance | Should render in < 5 s on a modern GPU |
| Color mapping | Viridis / BraggPeak gradient covers full range |
| Inspector pick | Click any trajectory → inspector updates |

*Regression note:* before the `std::map` fix in `BioSimRunner::loadCsv`,
loading this file always caused a segmentation fault (dangling pointer after
`std::vector<BioTrack>` reallocation). With the fix it must succeed.

---

## TC-06 · Graceful degradation (missing files)

**File:** `TC06_missing_files/visualization/manifest.json`

| What to check | Expected value |
|---|---|
| App does NOT crash | ✓ — must reach the workspace tab |
| Dashboard status | Shows "Missing file:" messages |
| All 4 chart scenes | Show "No data available" text (not blank/crash) |
| Focus curve chart | "No data available" |
| 3D OBJ viewer | Shows the stub two-point polyline |
| BioSim | Toolbar shows "(no file)"; Run shows warning dialog |
| Status bar | "Loaded: .../manifest.json" |

---

## Statistical validation

### Expected focus statistics for TC-01

Using a Gaussian beam model: w(z) = w₀ √(1 + ((z−z_f)/z_R)²)

| Quantity | Value |
|---|---|
| Beam waist w₀ | 0.5000 mm |
| Rayleigh range z_R | 40.00 mm |
| Focus position z_f | 150.0 mm |
| RMS radius at focus | 0.3536 mm (= w₀/√2) |
| Depth of focus (±z_R) | ±40 mm |
| Divergence half-angle | w₀/z_R = 12.5 mrad |
| Area at focus | π·w₀² = 7.854×10⁻⁷ m² |
| Area at z=0 | π·w(0)² = π·w₀²·(1+(150/40)²) = 2.576×10⁻⁴ m² |
| Area ratio (z=0 vs focus) | ≈ 327.8× |

### Expected BioSim statistics for TC-01 & TC-05

Proton beam in water, 150 MeV initial kinetic energy:

| Quantity | Value |
|---|---|
| Initial kinE | 150.0 MeV |
| CSDA range in water at 150 MeV | ≈ 157 mm (NIST PSTAR) |
| Simulated range (axial) | 150 mm (our z_end) |
| Mean energy deposit per step | ≈ 0.5–4.5 MeV (rising profile) |
| Bragg peak position | z ≈ 0.15 m (last 10% of track) |
| Steps per track | 30 |
| Total steps (TC-01) | 50 × 30 = 1500 |
| Total steps (TC-05) | 1200 × 30 = 36 000 |

---

## File structure

```
synthetic_dataset/
├── EXPECTED_RESULTS.md          ← this file
├── generate_synthetic_dataset.py
├── TC01_normal_beam/
│   ├── visualization/
│   │   ├── manifest.json
│   │   ├── trajectories_preview.csv   (50 traj × 40 steps = 2000 rows)
│   │   ├── trajectories_preview.obj   (50 polylines)
│   │   ├── focal_slice_points.csv     (200 points)
│   │   ├── envelope_rings.csv         (20 slices × 32 pts = 640 rows)
│   │   ├── beam_caustic_preview.obj   (ring mesh)
│   │   └── effective_lens_body_preview.obj
│   ├── tables/
│   │   ├── focus_curve.csv            (20 rows)
│   │   └── envelope_summary.csv       (20 rows)
│   └── energy_step_profile.csv        (50 tracks × 30 steps = 1500 rows)
├── TC02_tight_focus/
│   └── (same layout, tighter focus geometry)
├── TC03_no_focus/
│   └── (same layout, diverging beam)
├── TC04_edge_single_trajectory/
│   └── (1 trajectory, 2 steps, 1 focal point)
├── TC05_biosim_stress/
│   └── (50 preview trajectories, 1200 BioSim tracks)
└── TC06_missing_files/
    └── (manifest only, most files absent)
```
"""


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(description="Generate BeamLabStudio synthetic test data")
    parser.add_argument("--outdir", default=None,
                        help="Output directory (default: tests/synthetic_dataset/ "
                             "relative to this script)")
    args = parser.parse_args()

    script_dir = Path(__file__).parent
    outdir = Path(args.outdir) if args.outdir else script_dir / "synthetic_dataset"

    if outdir.exists():
        print(f"Removing existing {outdir} …")
        shutil.rmtree(outdir)

    outdir.mkdir(parents=True)
    print(f"Writing synthetic dataset to {outdir}\n")

    cases = [
        ("TC-01", "Normal converging beam",          tc01_normal_beam),
        ("TC-02", "Tight focus",                     tc02_tight_focus),
        ("TC-03", "No focus (diverging beam)",        tc03_no_focus),
        ("TC-04", "Edge: single two-step trajectory", tc04_edge_single_trajectory),
        ("TC-05", "BioSim stress (1200 tracks)",      tc05_edge_biosim_many_tracks),
        ("TC-06", "Graceful degradation (missing)",   tc06_edge_missing_files),
    ]

    for tag, desc, fn in cases:
        print(f"  {tag}  {desc} …", end=" ", flush=True)
        run = fn(outdir)
        print(f"→ {run.relative_to(outdir.parent)}")

    # Write documentation
    (outdir / "EXPECTED_RESULTS.md").write_text(EXPECTED_RESULTS_MD, encoding="utf-8")
    (outdir / "generate_synthetic_dataset.py").write_text(
        Path(__file__).read_text(encoding="utf-8"), encoding="utf-8"
    )

    print(f"\nDone.  {len(cases)} test cases written to:\n  {outdir}\n")
    print("Manifest files to open in BeamLabStudio:")
    for d in sorted(outdir.iterdir()):
        m = d / "visualization" / "manifest.json"
        if m.exists():
            print(f"  {m}")


if __name__ == "__main__":
    main()
