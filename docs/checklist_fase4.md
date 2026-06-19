## Checklist Fase 4 — Domain Unification

### MaterialRegistry

- [ ] **Compila sin errores ni warnings**: `cmake --build build --target beamlab_domain 2>&1 | grep -E "error:|warning:" | wc -l` retorna 0. Flags: `-Wall -Wextra -Wpedantic -Wconversion`
- [ ] **Búsqueda O(1) por nombre**: `get("water_icru")` retorna referencia válida. Benchmark: `materialRegistry.get("water_icru")` en loop de 10⁶ iteraciones promedia <100ns por call (<1μs con overhead de test)
- [ ] **Búsqueda O(1) por PDG code (ParticleRegistry)**: `getByPdgCode(2212)` retorna protón en un hash lookup
- [ ] **`findByCategory(Biological)`** retorna ≥20 materiales biológicos (ICRU-44 tissues). Verificar con: `EXPECT_GE(reg.findByCategory(MaterialCategory::Biological).size(), 20u)`
- [ ] **`findByCategory(Gas)`** retorna ≥3 gases (Air, Ar, He, CO₂). `EXPECT_GE(...size(), 3u)`
- [ ] **`findByProperty`**: lambda `[](const Material& m) { return m.density_g_cm3 > 10.0; }` retorna ≥3 (Pb, W, Au, Ta)
- [ ] **`addCustom()`**: material agregado es recuperable con `get()`. Persiste a disco en `~/.config/BeamLabStudio/custom_materials.json`. Verificar archivo existe después de `addCustom()`
- [ ] **`removeCustom()`**: material eliminado del registry y del archivo JSON. `removeCustom("custom_id")` retorna `true`. Segunda llamada retorna `false`
- [ ] **Override de built-in**: `addCustom()` con mismo id que built-in marca `isOverride = true`. El built-in original se preserva (el custom gana en `get()`, pero el built-in no se elimina)
- [ ] **`count()`**: después de `loadBuiltinMaterials()`, `count()` ≥ 40
- [ ] **`loadBuiltinMaterials()` es idempotente**: llamar dos veces produce mismo `count()` que llamar una vez
- [ ] **JSON round-trip**: `Material → nlohmann::json → Material` preserva todos los campos (id, name, density, Z_eff, A_eff, I, composition, sternheimer params)
- [ ] **`get()` lanza `std::out_of_range`** para id inexistente. `find()` retorna `std::nullopt` para id inexistente
- [ ] **Test completo**: `ctest -R test_MaterialRegistry --output-on-failure` pasa. Tests incluidos: lookup O(1), filter by category, filter by property, add/remove custom, override builtin, JSON serialization, load from JSON file

### ParticleRegistry

- [ ] **Compila sin errores** (cubierto por build beamlab_domain)
- [ ] **Búsqueda por PDG code**: `getByPdgCode(2212).name == "Proton"`, `getByPdgCode(11).name == "Electron"`. Ambos O(1)
- [ ] **Búsqueda por nombre**: `get("alpha")` retorna masas=3727.3794, charge=+2, WR=20.0
- [ ] **23 partículas built-in**: `loadBuiltinParticles()` precarga ≥21 especies. Verificar: electron, positron, muon±, tau±, proton, antiproton, neutron, deuteron, triton, He3, alpha, C12, N14, O16, pion±⁰, kaon±⁰, gamma
- [ ] **Categorías**: `findByCategory(ParticleCategory::Lepton)` ≥ 6 (e±, μ±, τ±). `findByCategory(ParticleCategory::Hadron)` ≥ 8. `findByCategory(ParticleCategory::Boson)` ≥ 1 (γ)
- [ ] **Índice dual consistente**: `get(nombre).pdgCode == getByPdgCode(get(nombre).pdgCode).pdgCode` para todas las partículas
- [ ] **`get()` lanza `std::out_of_range`** para nombre inexistente. `getByPdgCode(999999)` lanza `std::out_of_range`
- [ ] **JSON round-trip**: Particle → json → Particle preserva pdgCode, mass_MeV, charge_e, spin, WR
- [ ] **Test completo**: `ctest -R test_ParticleRegistry --output-on-failure` pasa

### SimulationEngine

- [ ] **Constructor acepta registries**: `SimulationEngine(MaterialRegistry&, ParticleRegistry&)` compila sin error
- [ ] **`computeStep(150.0, 0.01, "water_icru", "proton")`**: retorna `dEdx_MeV_cm > 0.0`. Verificar con `EXPECT_GT(step.dEdx_MeV_cm, 0.0)`
- [ ] **Precisión NIST PSTAR ±2% (target)**: `computeStep(150.0, 0.01, "water_icru", "proton")` produce dE/dx ≈ 3.37 MeV/cm. `EXPECT_NEAR(val, 3.37, 3.37 * 0.02)`. Si no alcanza, documentar desviación y tolerancia actual
- [ ] **Precisión NIST PSTAR ±2% multi-energía**: Verificar 50, 100, 150, 200 MeV contra tabla NIST. Dev <2% para E ≥ 50 MeV. Dev <5% para E = 10 MeV (shell corrections no modeladas)
- [ ] **`computeStep` clampa energyLoss**: `energyLoss_MeV <= kinE_MeV` para paso arbitrariamente largo (ej: `stepLength = 1000 cm, kinE = 0.1 MeV`)
- [ ] **`computeStep` para partícula neutra**: `computeStep(10.0, 0.1, "water_icru", "gamma")` retorna `dEdx = 0.0` (γ no tiene Bethe-Bloch)
- [ ] **`computeBraggCurve(50.0, "water_icru", "proton")`**: `peakDepth_cm > 0.0`, `peakDEdx_MeV_cm > 0.0`, `depth_cm.size() ≥ 2`
- [ ] **Bragg peak position**: `bragg.peakDepth_cm > 0.5 * bragg.depth_cm.back()` (peak ocurre cerca del final del rango)
- [ ] **Mayor energía → mayor profundidad**: `computeBraggCurve(150)"` tiene `peakDepth` mayor que `computeBraggCurve(50)`
- [ ] **`validateAgainstNist("water_icru", "proton")`**: `report.passed == true`. `report.referenceSource` contiene "NIST" o "PSTAR"
- [ ] **Material inexistente**: `computeStep(100, 0.01, "nonexistent", "proton")` lanza `std::out_of_range`
- [ ] **Partícula inexistente**: `computeStep(100, 0.01, "water_icru", "nonexistent")` lanza `std::out_of_range`
- [ ] **Energía cero/negativa**: `computeStep(0, ...)` y `computeStep(-1, ...)` retornan `dEdx = 0` sin crash
- [ ] **Test completo**: `ctest -R test_SimulationEngine --output-on-failure` pasa (15+ tests)

### Migración de Archivos

- [ ] **`src/biosim/physics/` → `src/domain/physics/`**: `StoppingPowerEngine.*`, `EnergyStraggling.*`, `BraggPeakCalculator.*` movidos (git mv, no copia). Verificar: `git log --follow src/domain/physics/StoppingPowerEngine.cpp` muestra historial completo
- [ ] **`src/biosim/materials/` → `src/domain/materials/`**: `MaterialRegistry.*`, `MaterialValidator.*` movidos. `BioMaterial.h` y `BioMaterialLibrary.h` reemplazados por shims
- [ ] **`src/biosim/geometry/` → `src/domain/geometry/`**: `PhantomLibrary.*`, `PhantomPreset.*` movidos. `BioSlab.h` → `Slab.h` (alias)
- [ ] **`src/biosim/core/BioSimRunner.*`** → `src/domain/simulation/BioSimRunner.*`
- [ ] **`src/biosim/core/EnergyScaleConverter.*`** → `src/domain/simulation/EnergyScaleConverter.*`
- [ ] **`src/biosim/core/BioSimConfig.h`, `BioSimResult.h`** → `src/domain/simulation/`
- [ ] **`src/biosim/core/ScoringPlane*`** → `src/services/analysis/engines/`
- [ ] **`src/biosim/ui/qt/*` → `src/ui/qt/widgets/*`**: 8 widgets movidos (EnergyColorMapper, EnergyScaleBar, BioViewport3D, TrajectoryInspectorPanel, SlabEditor3D, BioSimWidget, MaterialEditorDialog, PhantomPresetPanel)
- [ ] **Headers shim en `src/biosim/`**: `BioMaterial.h` (using alias + `[[deprecated]]`), `BioMaterialLibrary.h` (wrapper completo), `ParticleLibrary.h` (emulación), `StoppingPowerEngine.h` (using alias). Todos emiten `#pragma message("⚠️ DEPRECATED")`
- [ ] **Target `beamlab_biosim` eliminado**: `grep -c beamlab_biosim CMakeLists.txt` retorna 0
- [ ] **Target `beamlab_domain` agregado**: `src/domain/CMakeLists.txt` existe y define beamlab_domain como STATIC
- [ ] **Target `beamlab_services_analysis` agregado**: `src/services/CMakeLists.txt` existe
- [ ] **`beamlab_ui` linkea `beamlab_domain`**: `target_link_libraries(beamlab_ui ... beamlab_domain)` presente en CMakeLists.txt
- [ ] **`beamlab_tests` linkea `beamlab_domain`**: `target_link_libraries(beamlab_tests ... beamlab_domain)` presente en tests/unit/CMakeLists.txt

### Build y Compilación

- [ ] **Build completo sin warnings**: `cmake --build build -j$(nproc) 2>&1 | grep -E "warning:|error:" | wc -l` retorna 0
- [ ] **Build Release**: `cmake -B build-release -DCMAKE_BUILD_TYPE=Release && cmake --build build-release -j$(nproc)` completa sin errores en <150s
- [ ] **Build sin Qt**: `cmake -B build-headless -DBEAMLAB_ENABLE_QT_UI=OFF && cmake --build build-headless` compila beamlab_domain sin QApplication
- [ ] **Build con Qt**: `cmake -B build-ui -DBEAMLAB_ENABLE_QT_UI=ON && cmake --build build-ui` compila beamlab_domain + beamlab_ui + beamlab_services_analysis
- [ ] **Clang-tidy** (si está disponible): `run-clang-tidy src/domain/ -p build` sin nuevos warnings

### Tests

- [ ] **Regresión**: `ctest --test-dir build --output-on-failure` reporta todos los tests como PASS. Comparar contra baseline: `ctest -N` cuenta ≥45 tests
- [ ] **Tests de dominio**: `ctest -R "MaterialRegistry|ParticleRegistry|SimulationEngine" --output-on-failure` — todos PASS
- [ ] **Tests de física existentes**: `ctest -R "StoppingPower|Bragg" --output-on-failure` — tests legacy del biosim original siguen pasando
- [ ] **Sin regresión en tests de análisis**: `ctest -R "FrameStatistics|FocusDetector|SurfaceBuilder|BatchStatistics"` — todos PASS
- [ ] **Tests de plataforma**: `ctest -R "EventBus|CommandBus|JobScheduler"` — todos PASS

### UI y Verificación Visual

- [ ] **`MaterialEditorDialog`**: al abrir, lista completa de materiales por categoría (Biological, Gas, Metal, Plastic, Phantom). "+ New" crea material custom editable. "Delete" elimina solo custom materials. "Save" persiste a JSON
- [ ] **`SlabEditor3D`**: combo box de materiales lista solo `Biological` + `Phantom` (no metales ni gases, que no son relevantes para slabs). Al seleccionar material, el slab se actualiza
- [ ] **`BioSimWidget`**: toolbar muestra todas las opciones. "Browse CSV" abre file dialog. "Run" ejecuta simulación sin crash
- [ ] **MainWindow no incluye lógica de dominio**: `grep -r "StoppingPower\|BetheBloch\|BraggPeak\|BioMaterial" src/ui/qt/MainWindow.cpp | wc -l` retorna 0
- [ ] **`analysisPresenter_` usa `AnalysisOrchestrator`**: `grep "QProcess\|run_beamlab_full\|bash" src/ui/qt/MainWindow.cpp | wc -l` retorna 0 (no más dependencia de scripts externos)

### CI y Automatización

- [ ] **GitHub Actions**: `.github/workflows/build.yml` actualizado para incluir build de `beamlab_domain` en matrix (gcc + clang, Debug + Release)
- [ ] **Script de migración**: `scripts/migrate_biosim_to_domain.sh` existe, es ejecutable, y produce 6 commits limpios al correr en repo limpio
- [ ] **Shims documentados**: `docs/ARCHITECTURE.md` incluye sección Fase 4 con ADR-011 a ADR-014 y tabla de migración
- [ ] **Blueprint actualizado**: `BLUEPRINT.md` refleja estructura final de directorios

### Comandos de verificación rápida

```bash
# 1. Compilación
cmake --build build -j$(nproc) 2>&1 | grep -cE "error:|warning:" || echo "0 warnings"

# 2. Tests de dominio
ctest --test-dir build -R "MaterialRegistry|ParticleRegistry|SimulationEngine" --output-on-failure

# 3. Todos los tests
ctest --test-dir build --output-on-failure -j$(nproc)

# 4. Sin beamlab_biosim
grep -c "beamlab_biosim" CMakeLists.txt || echo "target beamlab_biosim eliminado"

# 5. beamlab_domain linkeado correctamente
grep -E "beamlab_domain" CMakeLists.txt src/domain/CMakeLists.txt tests/unit/CMakeLists.txt

# 6. MainWindow sin lógica de dominio
grep -cE "StoppingPower|BraggP|BioMaterial|ParticleLibrary" src/ui/qt/MainWindow.cpp || echo "MainWindow limpio"

# 7. Shims funcionan
echo '#include "biosim/materials/BioMaterial.h"' | g++ -x c++ - -c -I src -o /dev/null 2>&1 | head -3

# 8. Cobertura de nodos débilmente conectados (post-migración)
#   graphify query "What is the cohesion of the Material community?" --budget 500
```
