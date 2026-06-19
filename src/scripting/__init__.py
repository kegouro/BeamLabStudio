# BeamLabStudio Python package
#
# This __init__.py re-exports the C++ pybind11 module (_core) with
# additional Python-side helpers and convenience functions.
#
# Usage:
#   import beamlab
#   mats = beamlab.MaterialRegistry()
#   mats.load_builtin()

import os
import sys

# ── Import the native C++ module ────────────────────────────────────
# The pybind11 module is built as _core.so (or _core.cpython-3xx.so).
try:
    from . import _core
except ImportError:
    # Fallback: try direct import (for pip install or dev builds).
    try:
        import _core as _core_mod
        _core = _core_mod
    except ImportError:
        import warnings
        warnings.warn(
            "BeamLabStudio native module (_core) not found.\n"
            "Build with: cmake -B build -DBEAMLAB_ENABLE_PYTHON=ON && "
            "cmake --build build --target pybeamlab")
        _core = None

# ── Re-export all _core names ───────────────────────────────────────
if _core is not None:
    for _attr in dir(_core):
        if not _attr.startswith("_"):
            globals()[_attr] = getattr(_core, _attr)


# ── Python helpers ──────────────────────────────────────────────────

def load_default_materials():
    """Return a MaterialRegistry with all 55+ built-in materials."""
    reg = MaterialRegistry()
    reg.load_builtin()
    return reg


def load_default_particles():
    """Return a ParticleRegistry with all 23 built-in particles."""
    reg = ParticleRegistry()
    reg.load_builtin()
    return reg


def create_engine():
    """Return a SimulationEngine with default registries."""
    mats = load_default_materials()
    parts = load_default_particles()
    return SimulationEngine(mats, parts)


def bragg_peak(energy_mev: float,
               material: str = "water_icru",
               particle: str = "proton"):
    """Compute Bragg curve. Returns (depths_cm, dEdx_MeV_cm, peakDepth_cm)."""
    engine = create_engine()
    curve = engine.compute_bragg_curve(energy_mev, material, particle)
    return curve.depth_cm, curve.dEdx_MeV_cm, curve.peakDepth_cm


def demo():
    """Print a demo of available functionality."""
    mats = load_default_materials()
    parts = load_default_particles()
    print(f"Materials: {len(mats)} built-in")
    print(f"Particles: {len(parts)} built-in")
    print(f"  Water: ρ={mats['water_icru'].density_g_cm3} g/cm³, "
          f"I={mats['water_icru'].meanExcitationEnergy_eV} eV")
    p = parts.get_by_pdg(2212)
    print(f"  Proton: mass={p.mass_MeV} MeV, charge={p.charge_e}")

    engine = SimulationEngine(mats, parts)
    step = engine.compute_step(150.0, 0.01, "water_icru", "proton")
    print(f"  dE/dx @150 MeV: {step.dEdx_MeV_cm:.3f} MeV/cm")

    curve = engine.compute_bragg_curve(150.0, "water_icru", "proton")
    print(f"  Bragg peak at {curve.peakDepth_cm:.3f} cm")
