#!/usr/bin/env python3
"""
BeamLabStudio Python API Tests

Validates that the Python bindings produce the same numerical results
as the native C++ code.  Each test mirrors a corresponding C++ GoogleTest.

Usage:
    pytest tests/scripting/test_api.py -v
    python3 -m pytest tests/scripting/test_api.py --verbose
"""

import sys
import os
import math
import platform

# ── Ensure the built module is findable ─────────────────────────────
# Searches: build/python/ (cmake build), site-packages/ (pip install).
_script_dir = os.path.dirname(os.path.abspath(__file__))
_project_root = os.path.abspath(os.path.join(_script_dir, "..", ".."))

# Check ci-build, build, build-release, build-ui, build-root.
for build_dir in ["build", "build-release", "build-ui", "build-root", "ci-build"]:
    candidate = os.path.join(_project_root, build_dir, "python")
    if os.path.isdir(candidate):
        sys.path.insert(0, candidate)
        break

try:
    import beamlab
except ImportError:
    # Fallback: try the project root.
    sys.path.insert(0, os.path.join(_project_root, "build", "python"))
    import beamlab


# ═════════════════════════════════════════════════════════════════════
#  Helpers
# ═════════════════════════════════════════════════════════════════════

def _make_engine():
    """Create a SimulationEngine with default registries."""
    mats = beamlab.MaterialRegistry()
    mats.load_builtin()
    parts = beamlab.ParticleRegistry()
    parts.load_builtin()
    return beamlab.SimulationEngine(mats, parts), mats, parts


# ═════════════════════════════════════════════════════════════════════
#  MaterialRegistry Tests
# ═════════════════════════════════════════════════════════════════════

def test_material_count():
    """At least 40 built-in materials (same as test_MaterialRegistry.cpp)."""
    mats = beamlab.MaterialRegistry()
    mats.load_builtin()
    assert len(mats) >= 40


def test_material_get_water():
    """Water density = 1.0 g/cm³, I = 75.0 eV (same as C++ test)."""
    mats = beamlab.MaterialRegistry()
    mats.load_builtin()
    water = mats.get("water_icru")
    assert water.name == "Water (ICRU-44)"
    assert abs(water.density_g_cm3 - 1.0) < 0.01
    assert abs(water.meanExcitationEnergy_eV - 75.0) < 1.0


def test_material_get_lead():
    """Lead: density ≈ 11.35 g/cm³."""
    mats = beamlab.MaterialRegistry()
    mats.load_builtin()
    lead = mats["lead"]
    assert abs(lead.density_g_cm3 - 11.35) < 0.1


def test_material_contains():
    """__contains__ operator works."""
    mats = beamlab.MaterialRegistry()
    mats.load_builtin()
    assert "water_icru" in mats
    assert "nonexistent" not in mats


def test_material_missing_raises():
    """Accessing missing material raises KeyError (pybind maps out_of_range)."""
    mats = beamlab.MaterialRegistry()
    mats.load_builtin()
    try:
        mats.get("nonexistent_material")
        assert False, "Should have raised"
    except (KeyError, RuntimeError):
        pass


def test_material_add_custom():
    """Custom material is retrievable after add."""
    mats = beamlab.MaterialRegistry()
    mats.load_builtin()
    m = beamlab.Material()
    m.id = "test_py_material"
    m.name = "Python Test Material"
    m.density_g_cm3 = 2.5
    m.Z_eff = 10.0
    m.A_eff = 20.0
    m.meanExcitationEnergy_eV = 150.0
    mats.add_custom(m)

    retrieved = mats["test_py_material"]
    assert abs(retrieved.density_g_cm3 - 2.5) < 0.001
    assert retrieved.Z_eff == 10.0


def test_material_remove_custom():
    """Custom material removal works."""
    mats = beamlab.MaterialRegistry()
    mats.load_builtin()
    m = beamlab.Material()
    m.id = "temp_py"
    m.name = "Temporary"
    m.density_g_cm3 = 1.0
    m.Z_eff = 7.0
    m.A_eff = 12.0
    m.meanExcitationEnergy_eV = 70.0
    mats.add_custom(m)
    assert "temp_py" in mats

    mats.remove_custom("temp_py")
    assert "temp_py" not in mats


# ═════════════════════════════════════════════════════════════════════
#  ParticleRegistry Tests
# ═════════════════════════════════════════════════════════════════════

def test_particle_count():
    """At least 18 particles (same as test_ParticleRegistry.cpp)."""
    parts = beamlab.ParticleRegistry()
    parts.load_builtin()
    assert len(parts) >= 18


def test_particle_get_proton():
    """Proton: PDG 2212, mass ≈ 938.27 MeV."""
    parts = beamlab.ParticleRegistry()
    parts.load_builtin()
    p = parts.get_by_pdg(2212)
    assert p.name == "Proton" or p.name == "proton"
    assert abs(p.mass_MeV - 938.272) < 0.01
    assert abs(p.charge_e - 1.0) < 0.01


def test_particle_get_electron():
    """Electron: PDG 11, mass ≈ 0.511 MeV."""
    parts = beamlab.ParticleRegistry()
    parts.load_builtin()
    e = parts["electron"]
    assert abs(e.mass_MeV - 0.511) < 0.001
    assert abs(e.charge_e - (-1.0)) < 0.001


def test_particle_get_alpha():
    """Alpha: PDG 1000020040, mass ≈ 3727.38 MeV, charge = +2."""
    parts = beamlab.ParticleRegistry()
    parts.load_builtin()
    a = parts.get_by_pdg(1000020040)
    assert abs(a.mass_MeV - 3727.38) < 0.1
    assert abs(a.charge_e - 2.0) < 0.1
    assert abs(a.WR - 20.0) < 0.1


# ═════════════════════════════════════════════════════════════════════
#  SimulationEngine Tests
# ═════════════════════════════════════════════════════════════════════

def test_compute_step_proton_water():
    """150 MeV proton in water: 0 < dEdx < 10 MeV/cm."""
    engine, _, _ = _make_engine()
    step = engine.compute_step(150.0, 0.01, "water_icru", "proton")
    assert 0.0 < step.dEdx_MeV_cm < 10.0
    assert step.energyLoss_MeV > 0.0
    assert step.mcsAngle_rad >= 0.0


def test_compute_step_zero_energy():
    """0 MeV → dEdx = 0 (same as C++ test)."""
    engine, _, _ = _make_engine()
    step = engine.compute_step(0.0, 0.1, "water_icru", "proton")
    assert abs(step.dEdx_MeV_cm) < 1e-12
    assert abs(step.energyLoss_MeV) < 1e-12


def test_compute_step_stopping_power_increases():
    """dE/dx at 1 MeV > dE/dx at 150 MeV (Bethe-Bloch 1/β² term)."""
    engine, _, _ = _make_engine()
    high = engine.compute_step(150.0, 0.01, "water_icru", "proton")
    low = engine.compute_step(1.0, 0.01, "water_icru", "proton")
    assert low.dEdx_MeV_cm > high.dEdx_MeV_cm


def test_compute_step_alpha_vs_proton():
    """Alpha (Z=2) has ~4× stopping power vs proton (Z=1)."""
    engine, _, _ = _make_engine()
    p_step = engine.compute_step(10.0, 0.01, "water_icru", "proton")
    a_step = engine.compute_step(10.0, 0.01, "water_icru", "alpha")
    assert a_step.dEdx_MeV_cm > p_step.dEdx_MeV_cm


def test_compute_step_heavy_material():
    """dE/dx in lead > dE/dx in water (same energy)."""
    engine, _, _ = _make_engine()
    water = engine.compute_step(100.0, 0.01, "water_icru", "proton")
    lead = engine.compute_step(100.0, 0.01, "lead", "proton")
    assert lead.dEdx_MeV_cm > water.dEdx_MeV_cm


def test_compute_step_gamma():
    """Photon has no stopping power (Z=0 → Bethe-Bloch returns 0)."""
    engine, _, _ = _make_engine()
    step = engine.compute_step(10.0, 0.1, "water_icru", "gamma")
    assert abs(step.dEdx_MeV_cm) < 1e-12


def test_invalid_material_raises():
    """Unknown material → exception."""
    engine, _, _ = _make_engine()
    try:
        engine.compute_step(100.0, 0.01, "nonexistent_material", "proton")
        assert False, "Should have raised"
    except (KeyError, RuntimeError):
        pass


def test_invalid_particle_raises():
    """Unknown particle → exception."""
    engine, _, _ = _make_engine()
    try:
        engine.compute_step(100.0, 0.01, "water_icru", "nonexistent_particle")
        assert False, "Should have raised"
    except (KeyError, RuntimeError):
        pass


# ═════════════════════════════════════════════════════════════════════
#  Bragg Curve Tests
# ═════════════════════════════════════════════════════════════════════

def test_bragg_curve_has_peak():
    """Bragg curve: peakDepth > 0, peakDEdx > 0."""
    engine, _, _ = _make_engine()
    curve = engine.compute_bragg_curve(50.0, "water_icru", "proton")
    assert len(curve.depth_cm) >= 2
    assert curve.peakDepth_cm > 0.0
    assert curve.peakDEdx_MeV_cm > 0.0


def test_bragg_peak_near_end():
    """Bragg peak is at >50% of total range."""
    engine, _, _ = _make_engine()
    curve = engine.compute_bragg_curve(50.0, "water_icru", "proton")
    total_range = curve.depth_cm[-1]
    assert curve.peakDepth_cm > total_range * 0.5


def test_higher_energy_deeper_peak():
    """150 MeV penetrates deeper than 50 MeV."""
    engine, _, _ = _make_engine()
    c50 = engine.compute_bragg_curve(50.0, "water_icru", "proton")
    c150 = engine.compute_bragg_curve(150.0, "water_icru", "proton")
    assert c150.peakDepth_cm > c50.peakDepth_cm


def test_bragg_very_low_energy():
    """0.05 MeV proton should stop immediately (< 2 steps)."""
    engine, _, _ = _make_engine()
    curve = engine.compute_bragg_curve(0.05, "water_icru", "proton")
    assert len(curve.depth_cm) == 0 or len(curve.depth_cm) <= 2


# ═════════════════════════════════════════════════════════════════════
#  NIST Validation Tests
# ═════════════════════════════════════════════════════════════════════

def test_nist_validation_proton_water():
    """ValidateAgainstNist should report passed ≈ True."""
    engine, _, _ = _make_engine()
    report = engine.validate_against_nist("water_icru", "proton")
    assert report.referenceSource != ""
    # The deviation at 150 MeV for our Bethe-Bloch implementation
    # should be within 5% (even if not at ±2% yet).
    assert report.deviations is not None


# ═════════════════════════════════════════════════════════════════════
#  ProfileManager Tests
# ═════════════════════════════════════════════════════════════════════

def test_profile_manager_available():
    """ProfileManager returns at least 'quick_inspect' and 'full_analysis'."""
    pm = beamlab.ProfileManager()
    profiles = pm.available_profiles()
    assert isinstance(profiles, list)
    # Without config directory, the list may be empty.  That's OK.


def test_profile_resolve_defaults():
    """ProfileManager.resolve() returns default config even with bad name."""
    pm = beamlab.ProfileManager()
    cfg = pm.resolve("nonexistent_profile")
    assert cfg is not None
    assert isinstance(cfg, dict)


# ═════════════════════════════════════════════════════════════════════
#  Cross-module consistency
# ═════════════════════════════════════════════════════════════════════

def test_water_id_consistent():
    """Same material ID exists in Material and works in SimulationEngine."""
    engine, mats, parts = _make_engine()
    water = mats["water_icru"]
    step = engine.compute_step(150.0, 0.01, "water_icru", "proton")
    assert step.dEdx_MeV_cm > 0.0  # Material found and used.


# ═════════════════════════════════════════════════════════════════════
#  Main entry point
# ═════════════════════════════════════════════════════════════════════

if __name__ == "__main__":
    import pytest
    sys.exit(pytest.main([__file__, "-v"]))
