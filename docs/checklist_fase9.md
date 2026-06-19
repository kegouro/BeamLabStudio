## Checklist Fase 9 — Python API

### Build

- [ ] **pybind11 module compiles**: `cmake -B build -DBEAMLAB_ENABLE_PYTHON=ON && cmake --build build --target pybeamlab`. El archivo `build/python/beamlab/_core.so` existe y es un ELF/DLL válido.
- [ ] **Module naming**: `file build/python/beamlab/_core.so` retorna `ELF 64-bit LSB shared object`. El nombre estándar es `_core.cpython-3xx-x86_64-linux-gnu.so` para Python 3.12+.
- [ ] **__init__.py alongside .so**: `ls build/python/beamlab/` muestra `__init__.py` + `_core.so` en el mismo directorio.
- [ ] **pip install completes**: `pip install .` desde la raíz del repo sin errores. Verificar con: `pip install -e . 2>&1 | grep -i error` retorna vacío.
- [ ] **pip install produce beamlab package**: `python3 -c "import beamlab; print(beamlab.__file__)"` retorna un path válido (no `ImportError`).
- [ ] **import beamlab sin LD_LIBRARY_PATH**: `env -u LD_LIBRARY_PATH python3 -c "import beamlab; beamlab.demo()"` funciona. La RPATH embedida (`$ORIGIN`) resuelve las dependencias.
- [ ] **Soporte multi-plataforma**: El módulo compila en Linux (`.so`), macOS (`.dylib`), y Windows (`.pyd`). Verificar en CI matrix si es posible.

### Funcionalidad — MaterialRegistry

- [ ] **`MaterialRegistry().load_builtin()`** carga ≥55+ materiales ICRU/NIST. Verificar: `len(mats) >= 40`.
- [ ] **`get("water_icru")`** retorna material con `density_g_cm3 ≈ 1.0` y `meanExcitationEnergy_eV ≈ 75.0`. O(1) lookup.
- [ ] **`find("nonexistent")`** retorna `None` (no exception). `get("nonexistent")` lanza `KeyError`.
- [ ] **`add_custom(Material(...))`** agrega material. `"custom_id" in mats` retorna `True` después de agregar.
- [ ] **`remove_custom("custom_id")`** retorna `True`. Material built-in no se puede remover (retorna `False`).
- [ ] **`names()`** retorna `List[str]` con todos los IDs registrados.
- [ ] **`__getitem__`**: `mats["water_icru"]` funciona como `get()`.

### Funcionalidad — ParticleRegistry

- [ ] **`ParticleRegistry().load_builtin()`** carga ≥23 partículas PDG 2022. Verificar: `len(parts) >= 18`.
- [ ] **`get_by_pdg(2212)`** retorna protón con `mass_MeV ≈ 938.272`, `charge_e = 1.0`.
- [ ] **`get_by_pdg(11)`** retorna electrón con `mass_MeV ≈ 0.511`, `charge_e = -1.0`.
- [ ] **`get_by_pdg(1000020040)`** retorna partícula α con `mass_MeV ≈ 3727.38`, `WR = 20.0`.
- [ ] **`get("electron")`**, `get("proton")`, `get("alpha")` funcionan por nombre.
- [ ] **PDG inexistente**: `get_by_pdg(999999)` lanza excepción.

### Funcionalidad — SimulationEngine

- [ ] **`compute_step(150, 0.01, "water_icru", "proton")`** retorna `StepResult` con `0 < dEdx_MeV_cm < 10`.
- [ ] **Energía cero**: `compute_step(0, 0.1, ...)` retorna `dEdx_MeV_cm ≈ 0`, `energyLoss_MeV ≈ 0`.
- [ ] **dE/dx 1 MeV > dE/dx 150 MeV**: `compute_step(1, ...)` > `compute_step(150, ...)` (término 1/β²).
- [ ] **Alpha vs protón**: `compute_step(10, ..., "alpha")` tiene ~4× dE/dx de `"proton"`.
- [ ] **Material denso > liviano**: `compute_step(100, ..., "lead")` > `compute_step(100, ..., "water")`.
- [ ] **Partícula neutra**: `compute_step(10, ..., "gamma")` retorna `dEdx ≈ 0` (Bethe-Bloch para Z=0).
- [ ] **Material inexistente lanza excepción**: `compute_step(100, ..., "nonexistent", "proton")` → `KeyError` o `RuntimeError`.
- [ ] **Partícula inexistente lanza excepción**: similar.

### Funcionalidad — Bragg Curve

- [ ] **`compute_bragg_curve(50, "water_icru", "proton")`**: `peakDepth_cm > 0`, `peakDEdx_MeV_cm > 0`, `len(depth_cm) >= 2`.
- [ ] **Peak cerca del final**: `peakDepth_cm > totalRange * 0.5`.
- [ ] **Mayor energía → mayor profundidad**: `peakDepth(150) > peakDepth(50)`.
- [ ] **Energía muy baja**: `compute_bragg_curve(0.05, ...)` retorna 0 o ≤2 pasos.

### Funcionalidad — ProfileManager y AnalysisOrchestrator

- [ ] **`ProfileManager().available_profiles()`**: retorna lista (puede estar vacía si no hay perfiles instalados — es válido).
- [ ] **`ProfileManager().resolve("nonexistent")`**: retorna dict sin crash.
- [ ] **`AnalysisOrchestrator` se construye**: `AnalysisOrchestrator()` no lanza (aunque no tenga todos los registries inyectados).
- [ ] **`AnalysisOrchestrator.run()` con callback**: acepta `on_progress=lambda p, s: None`.
- [ ] **`Orchestrator.cancel()`**: no lanza incluso si no hay análisis corriendo.

### Precisión Numérica

- [ ] **Resultados Python ≈ C++**: comparar `compute_step(150, 0.01, "water_icru", "proton")` entre Python y C++. `|dEdx_py - dEdx_cpp| / dEdx_cpp < 1e-12`.
- [ ] **`test_api.py` pasa**: `pytest tests/scripting/test_api.py -v` retorna 27/27 passed.
- [ ] **CI ejecuta tests Python**: `.github/workflows/build.yml` incluye step de `pytest tests/scripting/`.
- [ ] **Sin regresión en valores físicos**: comparar salidas de validate_against_nist contra baseline de C++ test suite.

### Documentación

- [ ] **`docs/API_REFERENCE.md` cubre todas las clases**: MaterialRegistry, ParticleRegistry, SimulationEngine, AnalysisOrchestrator, ProfileManager, Material, Particle, StepResult, BraggCurve, ValidationReport, Element. Verificar con: `grep "^## " docs/API_REFERENCE.md` cuenta las 11 secciones.
- [ ] **Type hints documentados**: cada método en las tablas incluye `str`, `int`, `float`, `List[str]`, etc.
- [ ] **Ejemplos por clase**: cada tabla tiene 2-3 líneas de código Python debajo (`**Usage:**`).
- [ ] **`src/scripting/examples/quickstart.ipynb` ejecuta sin errores**: `jupyter nbconvert --to notebook --execute quickstart.ipynb`. Todas las celdas completadas sin crash.
- [ ] **Notebook produce gráfico**: salida de celda 9 incluye imagen PNG con 5 curvas Bragg.
- [ ] **`src/scripting/examples/README.md`** incluye instrucciones de `pip install` y `jupyter`.

### Regresión

- [ ] **Build sin Qt**: `cmake -B build-headless -DBEAMLAB_ENABLE_PYTHON=ON`. Compila sin errores. `_core.so` producido sin levantar QApplication.
- [ ] **Build con Qt**: `cmake -B build -DBEAMLAB_ENABLE_QT_UI=ON -DBEAMLAB_ENABLE_PYTHON=ON`. Compila. Ambos targets (`beamlab_ui` + `pybeamlab`) producidos.
- [ ] **ctest sin regresiones**: `ctest --test-dir build -L unit --output-on-failure` reporta todos los tests PASS.
- [ ] **Sin warnings nuevos**: `cmake --build build 2>&1 | grep -cE "warning:"` no aumentó vs Fase 8.
- [ ] **Python module independiente**: `python3 -c "import beamlab"` funciona SIN tener BeamLabStudio compilado ni en LD_LIBRARY_PATH (solo el `.so` instalado vía pip o cmake install).

### Comandos de verificación rápida

```bash
# 1. Build Python module
cmake -B build -DBEAMLAB_ENABLE_PYTHON=ON -DBEAMLAB_ENABLE_QT_UI=OFF
cmake --build build --target pybeamlab -j$(nproc)
find build/python -name '*.so' -o -name '*.pyd'

# 2. Import test
PYTHONPATH=build/python:$PYTHONPATH python3 -c "import beamlab; beamlab.demo()"

# 3. Python tests
PYTHONPATH=build/python:$PYTHONPATH pytest tests/scripting/test_api.py -v

# 4. Notebook test
pip install jupyter nbconvert
PYTHONPATH=build/python:$PYTHONPATH \
  jupyter nbconvert --to notebook --execute src/scripting/examples/quickstart.ipynb

# 5. Without LD_LIBRARY_PATH
env -u LD_LIBRARY_PATH \
  PYTHONPATH=build/python:$PYTHONPATH \
  python3 -c "import beamlab; print('OK')"

# 6. C++ test regression
ctest --test-dir build -L unit --output-on-failure | tail -3
```
