## Checklist Fase 8 — Testing Fortress

### Tests Unitarios (>40 tests totales)

- [ ] **`test_EventBus` pasa**: `ctest -R "EventBusTest" --verbose` reporta 10/10 tests. Verifica: publish/subscribe, unsubscribe stops delivery, multiple subscribers, no crash sin subscribers, ScopedSubscription auto-unsubscribe, event data correcto, subscriberCount, clear.
- [ ] **`test_CommandBus` pasa**: `ctest -R "CommandBusTest" --verbose` reporta 8/8 tests. Verifica: dispatch registered handler, multiple types, unregistered throw, double registration throw, hasHandler, clear, string command, handler captures correct value.
- [ ] **`test_PluginHost` pasa**: `ctest -R "PluginHostTest" --verbose` reporta 14/14 tests. Verifica: builtin registration, null registration, multiple builtins, getPluginsOfType, initialize/shutdown, cannot unload builtin, load .so (skip si no hay compilador), load missing returns nullopt.
- [ ] **`test_MaterialRegistry` pasa**: `ctest -R "MaterialRegistry" --verbose`. 18 tests. Verifica: count ≥40, O(1) get, find returns nullopt, filter by category (gas ≥3, metal ≥7), add/remove custom, override builtin, JSON serialization, load from JSON file.
- [ ] **`test_ParticleRegistry` pasa**: `ctest -R "ParticleRegistry" --verbose`. 18 tests. Verifica: getByPdgCode(2212)=proton, 23 species, find by category (lepton ≥6, ion ≥5), dual index consistency, JSON round-trip, missing throws.
- [ ] **`test_SimulationEngine` pasa**: `ctest -R "SimulationEngine" --verbose`. 26 tests. Verifica: computeStep water+proton, zero KE, invalid material/particle throws, stopping power increases near Bragg peak, heavier material more SP, alpha vs proton, Bragg curve peak, NIST validation passed, MCS angle, photon has no SP.
- [ ] **`test_ImporterRegistry` pasa**: `ctest -R "ImporterRegistry" --verbose`. 12 tests. Verifica: Geant4 CSV score ≥0.95, COMSOL ≥0.85, ROOT magic bytes 1.0, delimited fallback 0.30, no match → bestMatch=nullptr, import throws on unknown format, alternatives populated, empty file.
- [ ] **`test_DockManager` pasa**: solo si Qt está disponible. Verifica: registerDockable, saveLayout returns non-empty QByteArray, restoreLayout no crash, createViewMenu generates checkable actions, toggleDockable, resetToDefaults.
- [ ] **`test_JobScheduler` pasa**: `ctest -R "JobScheduler" --verbose`. 6 tests. Verifica: 3 engines correct order (A,B→C), engines run in parallel, engine failure cancels others, cancellation stops execution, single level, empty engine list.
- [ ] **`test_QueryCache` pasa**: `ctest -R "QueryCache" --verbose`. 13 tests. Verifica: miss returns nullopt, put→get returns value, overwrite gets latest, TTL=0 returns expired, LRU eviction, invalidateByPrefix, invalidateAll, struct serialization, concurrent put+get, concurrent invalidate.
- [ ] **`test_ProfileManager` pasa**: `ctest -R "ProfileManager" --verbose`. 10 tests. Verifica: availableProfiles, loadProfile con overrides, resolve merges default→profile→cli, deep merge objects, saveUserOverrides crea archivo, user overrides en resolve.
- [ ] **`test_AnalysisPresenter` pasa**: `ctest -R "AnalysisPresenter" --verbose`. 9 tests. Verifica: constructor no throw, startAnalysis emits started, cancel no crash, loadProfile emits profilesChanged, filters empty when no registry, formats empty, setImporterRegistry enables filters.
- [ ] **`test_ExporterRegistry` pasa**: `ctest -R "ExporterRegistry" --verbose`. 12 tests. Verifica: available formats, CSV creates file with header, export content valid, exportAll creates subdirectories, specific formats, unknown format throws, progress callback fires.
- [ ] **`test_ParquetExporter` pasa**: `ctest -R "ParquetExporter" --verbose`. 8 tests. Verifica: interim .parquet.csv created, header correct, data rows match, progress 0→1, warning logged, name/format consistent.
- [ ] **Total tests unitarios**: `ctest -L unit --output-on-failure | tail -3`. Cuenta **______ / >40** tests.

### Tests Integración

- [ ] **`test_full_pipeline::ImportAnalyzeExport_SmallCSV`**: CSV 100×100 (10k samples) → import Geant4 → analyze (JobScheduler) → export CSV. Verificar: `totalSamples == 10000`, `bestMatch != nullptr`, export CSV con `bytesWritten > 0`.
- [ ] **`test_full_pipeline::BioSim_WaterProton`**: `SimulationEngine::computeBraggCurve(150, water, proton)` → `peakDepth > 0`, `peakDEdx > 0`, peak en 40-100% del rango, mayor energía → peak más profundo.
- [ ] **`test_full_pipeline::PluginDiscovery_Runtime`**: PluginHost con builtin dummy. `loadedCount == 1`, `getPluginsOfType<IPlugin>()` lo encuentra, `unload()` en builtin retorna false.
- [ ] **`test_full_pipeline::AnalysisCancellation`**: 500×500 (250k samples), análisis en `std::async`, cancelar a los 30ms. Sin deadlock, sin crash. `progressCalls >= 0`.
- [ ] **`test_plugin_discovery`** (existente): 11 tests. Builtin importers/exporters, detección por extensión, .so loading, corrupt .so no crash.
- [ ] **Total tests integración**: `ctest -L integration --output-on-failure | tail -3`. Cuenta **______ / 15** tests.

### Cobertura

- [ ] **`src/domain/` >85%**: `gcov --json` → `src/domain/` coverage report. Verificar con: `cmake --build build --target coverage && cat build/coverage/index.html | grep domain`.
- [ ] **`src/services/` >70%**: `gcov --json` → `src/services/` coverage report.
- [ ] **`src/platform/` >60%**: `gcov --json` → `src/platform/` coverage report.
- [ ] **Coverage en CI**: GitHub Actions Debug+gcc job publica `coverage/index.html` como artifact descargable.
- [ ] **Sin regresión de cobertura**: comparar coverage actual vs baseline de Fase 7. Ningún módulo bajó >5%.

### CI / Build

- [ ] **GitHub Actions matrix pasa**: 4 configs: Debug+gcc-13, Debug+clang-18, Release+gcc-13, Release+clang-18. Todas PASS.
- [ ] **Unit tests en CI**: `ctest -L unit --output-on-failure` reporta 0 failures en todas las configs.
- [ ] **Integration tests en CI** (solo Release): `ctest -L integration --output-on-failure` reporta 0 failures.
- [ ] **Benchmarks en CI** (solo Release): `ctest -L performance --verbose` ejecuta sin crash. Output capturado para comparación histórica.
- [ ] **Coverage artifact**: CI Debug+gcc job: `cmake --build build --target coverage` → sube `build/coverage/` como artifact.
- [ ] **Lint clang-format**: `find src/ tests/ -name '*.cpp' -o -name '*.h' | xargs clang-format-18 --dry-run --Werror` retorna 0.
- [ ] **Build sin warnings nuevos**: `cmake --build build 2>&1 | grep -cE "warning:"` no aumentó vs Fase 7.

### DevEx

- [ ] **`.clang-format` aplicado**: `find src/ tests/ -name '*.cpp' -o -name '*.h' | xargs clang-format-18 -i` no produce cambios (ya formateado). Verificar: `git diff --stat` retorna 0 líneas.
- [ ] **`CONTRIBUTING.md` completo**: build instructions (4 comandos), git workflow (fork→branch→PR), code style guide, test structure, code review checklist (12 items), project layers diagram.
- [ ] **Build + test time**: siguiendo `CONTRIBUTING.md`, un nuevo desarrollador puede clonar, build y ejecutar tests en <10 minutos. Verificar con:
  ```bash
  time git clone <url> bls && cd bls && cmake -B build && cmake --build build -j$(nproc) && ctest --test-dir build
  ```

### Regresión

- [ ] **`ctest` sin segfaults**: `ctest --test-dir build --output-on-failure -j$(nproc)` reporta todos los tests PASS. 0 crashes, 0 segfaults.
- [ ] **Sin memory leaks** (si ASan disponible): `cmake -B build-asan -DCMAKE_BUILD_TYPE=Debug -DSANITIZE_ADDRESS=ON && ctest --test-dir build-asan` — 0 leaks reportados.
- [ ] **Aplicación funcionalmente equivalente a v0.2.0**: smoke test manual:
  - App arranca sin crash
  - File → Open muestra diálogo con filtros dinámicos
  - Tab Analysis muestra 3D viewport
  - Tab BioSim muestra controles
  - Tab Export lista formatos

### Comandos de verificación rápida

```bash
# 1. Todos los tests
ctest --test-dir build --output-on-failure -j$(nproc) 2>&1 | tail -5

# 2. Unit tests count
ctest --test-dir build -L unit 2>&1 | grep "tests\s*from"

# 3. Integration tests
ctest --test-dir build -L integration --output-on-failure

# 4. Performance benchmarks
ctest --test-dir build -L performance --output-on-failure

# 5. Coverage (Debug only)
cmake -B build -DENABLE_COVERAGE=ON && cmake --build build --target coverage

# 6. Lint
find src/ tests/ -name '*.cpp' -o -name '*.h' | xargs clang-format-18 --dry-run --Werror 2>&1 | grep -c "error:" || echo "Lint passed"

# 7. Sin warnings
cmake --build build 2>&1 | grep -cE "warning:" || echo "0 warnings"
```
