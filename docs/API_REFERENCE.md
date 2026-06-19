# BeamLabStudio Python API Reference

> Version 3.0.0 — Generated from `beamlab._core` (pybind11 module).

---

## Installation

```bash
# From source (cmake build).
cmake -B build-ui -DBEAMLAB_ENABLE_PYTHON=ON -DBEAMLAB_ENABLE_QT_UI=ON
cmake --build build-ui --target pybeamlab
export PYTHONPATH="$PWD/build-ui/python:$PYTHONPATH"

# From PyPI (future).
pip install beamlab
```

```python
import beamlab
beamlab.demo()
```

---

## Top-Level Functions

| Function | Returns | Description |
|----------|---------|-------------|
| `load_default_materials()` | `MaterialRegistry` | Returns registry with 55+ materials loaded |
| `load_default_particles()` | `ParticleRegistry` | Returns registry with 23 PDG particles |
| `create_engine()` | `SimulationEngine` | Engine with default registries |
| `bragg_peak(energy, material, particle)` | `tuple` | Quick Bragg: `(depths, dEdx, peakDepth)` |
| `demo()` | `None` | Print available functionality |

---

## `beamlab.MaterialRegistry`

Container for material data.  Query by name or ID in O(1).

```python
mats = beamlab.MaterialRegistry()
mats.load_builtin()
water = mats["water_icru"]
```

| Method | Returns | Description |
|--------|---------|-------------|
| `__init__()` | `MaterialRegistry` | Empty registry |
| `load_builtin()` | `None` | Load 55+ ICRU/NIST materials |
| `get(id: str)` | `Material` | Get material by ID; raises `KeyError` if missing |
| `find(id: str)` | `Optional[Material]` | Safe get; returns `None` if missing |
| `names()` | `List[str]` | All registered material IDs |
| `count()` | `int` | Number of materials |
| `add_custom(mat: Material)` | `None` | Add or override a material |
| `remove_custom(id: str)` | `bool` | Remove a custom material; `False` if built-in |
| `__len__()` | `int` | Alias for `count()` |
| `__getitem__(id: str)` | `Material` | Alias for `get()` |
| `__contains__(id: str)` | `bool` | Check if ID is registered |

**Usage:**
```python
mats = beamlab.MaterialRegistry()
mats.load_builtin()
print(len(mats))               # ≥ 40
water = mats["water_icru"]      # O(1) lookup
mats.add_custom(Material(...))  # user-defined material
```

---

## `beamlab.ParticleRegistry`

Container for particle species indexed by name and PDG code.

```python
parts = beamlab.ParticleRegistry()
parts.load_builtin()
proton = parts.get_by_pdg(2212)
```

| Method | Returns | Description |
|--------|---------|-------------|
| `__init__()` | `ParticleRegistry` | Empty registry |
| `load_builtin()` | `None` | Load 23 PDG 2022 particle species |
| `get(name: str)` | `Particle` | By name; raises if missing |
| `get_by_pdg(code: int)` | `Particle` | By PDG Monte Carlo number |
| `names()` | `List[str]` | All registered particle names |
| `count()` | `int` | Number of particles |
| `__len__()` | `int` | Alias for `count()` |
| `__getitem__(name: str)` | `Particle` | Alias for `get()` |

**Usage:**
```python
parts = beamlab.ParticleRegistry()
parts.load_builtin()
proton = parts.get_by_pdg(2212)   # PDG code
electron = parts["electron"]      # by name
```

---

## `beamlab.Material`

Physical properties of a material for stopping-power calculations.

| Field | Type | Description |
|-------|------|-------------|
| `id` | `str` | Unique identifier, e.g. `"water_icru"` |
| `name` | `str` | Display name, e.g. `"Water (ICRU-44)"` |
| `density_g_cm3` | `float` | Mass density [g/cm³] |
| `Z_eff` | `float` | Effective atomic number |
| `A_eff` | `float` | Effective atomic mass [g/mol] |
| `meanExcitationEnergy_eV` | `float` | Mean excitation energy I [eV] |
| `radiationLength_cm` | `float` | Radiation length X₀ [cm] |
| `composition` | `List[Element]` | Elemental composition (mass fractions) |
| `WR` | `float` | Radiation weighting factor (ICRP-103) |
| `WT` | `float` | Tissue weighting factor |
| `state` | `int` | 0=solid, 1=liquid, 2=gas |

| Method | Returns | Description |
|--------|---------|-------------|
| `is_gas()` | `bool` | True if state == 2 |
| `is_valid()` | `bool` | True if physics parameters are consistent |

**Usage:**
```python
w = mats["water_icru"]
print(f"ρ = {w.density_g_cm3:.3f} g/cm³, I = {w.meanExcitationEnergy_eV:.1f} eV")
```

---

## `beamlab.Particle`

Physical properties of a particle species.

| Field | Type | Description |
|-------|------|-------------|
| `name` | `str` | Display name, e.g. `"Proton"` |
| `pdgCode` | `int` | PDG Monte Carlo number |
| `mass_MeV` | `float` | Rest mass [MeV/c²] |
| `charge_e` | `float` | Electric charge in units of \|e\| |
| `spin` | `float` | Spin (units of ℏ/2) |
| `WR` | `float` | Radiation weighting factor |

---

## `beamlab.SimulationEngine`

Unified physics engine for stopping power, straggling, MCS, and Bragg curves.

```python
engine = beamlab.SimulationEngine(mats, parts)
step = engine.compute_step(150.0, 0.01, "water_icru", "proton")
```

| Method | Returns | Description |
|--------|---------|-------------|
| `__init__(mats, parts)` | `SimulationEngine` | Inject registries (required) |
| `compute_step(kinE_MeV, stepLength_cm, material_name, particle_name)` | `StepResult` | Single step: dE/dx, MCS, straggling |
| `compute_bragg_curve(initialE_MeV, material_name, particle_name, n_steps=1000)` | `BraggCurve` | Depth-dose profile |
| `validate_against_nist(material_name, particle_name)` | `ValidationReport` | Compare against NIST PSTAR |

**Parameters:**
- `kinE_MeV` / `initialE_MeV` — kinetic energy in MeV
- `stepLength_cm` — step size in cm (default 0.01)
- `material_name` — material ID (e.g. `"water_icru"`)
- `particle_name` — particle name (e.g. `"proton"`)
- `n_steps` — number of steps for Bragg integration (default 1000)

---

## `beamlab.StepResult`

Result of a single `compute_step()` call.

| Field | Type | Description |
|-------|------|-------------|
| `dEdx_MeV_cm` | `float` | Linear stopping power [MeV/cm] |
| `energyLoss_MeV` | `float` | Energy lost in this step [MeV] |
| `mcsAngle_rad` | `float` | Multiple Coulomb scattering angle [rad] |
| `mcsDisplacement_cm` | `float` | Lateral MCS displacement [cm] |
| `stragglingSigma_MeV` | `float` | Energy straggling sigma [MeV] |

---

## `beamlab.BraggCurve`

Result of `compute_bragg_curve()`.

| Field | Type | Description |
|-------|------|-------------|
| `depth_cm` | `List[float]` | Depth along the beam axis [cm] |
| `dEdx_MeV_cm` | `List[float]` | Stopping power at each depth [MeV/cm] |
| `peakDepth_cm` | `float` | Depth of the Bragg peak [cm] |
| `peakDEdx_MeV_cm` | `float` | Stopping power at the peak [MeV/cm] |

**Usage:**
```python
curve = engine.compute_bragg_curve(150.0, "water_icru", "proton")
print(f"Bragg peak at z = {curve.peakDepth_cm:.2f} cm")
```

---

## `beamlab.ValidationReport`

Result of `validate_against_nist()`.

| Field | Type | Description |
|-------|------|-------------|
| `passed` | `bool` | True if deviation ≤ tolerance |
| `referenceSource` | `str` | Name of the reference dataset |
| `deviations` | `List[Tuple[str, float]]` | (quantity, deviation%) pairs |

**Usage:**
```python
report = engine.validate_against_nist("water_icru", "proton")
print(f"Validation: {'PASS' if report.passed else 'FAIL'}")
```

---

## `beamlab.ProfileManager`

Load and merge JSON analysis profiles.

```python
pm = beamlab.ProfileManager()
cfg = pm.resolve("quick_inspect")
```

| Method | Returns | Description |
|--------|---------|-------------|
| `__init__()` | `ProfileManager` | Default paths |
| `load_profile(name: str)` | `dict` | Raw JSON profile from file |
| `available_profiles()` | `List[str]` | Profile names (without `.json`) |
| `resolve(profile_name, cli_overrides=None)` | `dict` | Merged config: default → profile → user → CLI |

**Merge hierarchy (last wins):**
1. `config/default_analysis.json`
2. `config/profiles/<name>.json`
3. `~/.config/BeamLabStudio/<name>.json`
4. CLI overrides (if provided)

---

## `beamlab.AnalysisOrchestrator`

Full analysis pipeline (import → analyze → export).

```python
orch = beamlab.AnalysisOrchestrator()
result = orch.run("/data/run.csv", config, on_progress=print)
```

| Method | Returns | Description |
|--------|---------|-------------|
| `__init__()` | `AnalysisOrchestrator` | Requires pre-configured registries |
| `run(path, config, on_progress)` | `AnalysisResult` | Execute pipeline |
| `cancel()` | `None` | Stop running analysis |

**Requires:** `ImporterRegistry`, `ExporterRegistry`, `JobScheduler`, `StorageManager`, `EventBus`.

---

## `beamlab.Material`

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `id` | `str` | `""` | Unique key |
| `name` | `str` | `""` | Display name |
| `symbol` | `str` | `""` | Chemical symbol |
| `density_g_cm3` | `float` | `1.0` | Density [g/cm³] |
| `Z_eff` | `float` | `7.22` | Effective atomic number |
| `A_eff` | `float` | `12.01` | Effective atomic mass [g/mol] |
| `meanExcitationEnergy_eV` | `float` | `75.0` | I-value [eV] |
| `radiationLength_cm` | `float` | `36.08` | X₀ [cm] |
| `WR` | `float` | `1.0` | Radiation weighting factor |
| `WT` | `float` | `0.0` | Tissue weighting factor |
| `composition` | `list[Element]` | `[]` | Elemental composition |
| `state` | `int` | `1` | 0=solid, 1=liquid, 2=gas |

---

## `beamlab.Element`

| Field | Type | Description |
|-------|------|-------------|
| `symbol` | `str` | Chemical symbol, e.g. `"H"` |
| `Z` | `int` | Atomic number |
| `massFraction` | `float` | Mass fraction in material [0–1] |

---

## `beamlab.Particle`

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `name` | `str` | `""` | Display name |
| `pdgCode` | `int` | `0` | PDG Monte Carlo number |
| `mass_MeV` | `float` | `0.0` | Rest mass [MeV/c²] |
| `charge_e` | `float` | `0.0` | Charge [e] |
| `spin` | `float` | `0.0` | Spin [ℏ/2] |
| `WR` | `float` | `1.0` | Radiation weighting factor |

---

## Index of All Classes

| Class | Module | Key Method |
|-------|--------|------------|
| `MaterialRegistry` | `beamlab` | `get(id)` |
| `ParticleRegistry` | `beamlab` | `get_by_pdg(code)` |
| `SimulationEngine` | `beamlab` | `compute_step(E, dx, mat, part)` |
| `StepResult` | `beamlab` | — data class |
| `BraggCurve` | `beamlab` | — data class |
| `ValidationReport` | `beamlab` | — data class |
| `ProfileManager` | `beamlab` | `resolve(name, cli)` |
| `AnalysisOrchestrator` | `beamlab` | `run(path, config)` |
| `Material` | `beamlab` | — data class (set fields) |
| `Particle` | `beamlab` | — data class |
| `Element` | `beamlab` | — data class |

---

*For C++ API reference, see `docs/ARCHITECTURE.md` and `docs/PLUGIN_DEVELOPMENT.md`.*
