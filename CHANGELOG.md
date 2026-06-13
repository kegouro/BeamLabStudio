# Changelog

All notable changes to BeamLabStudio are documented here.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [Unreleased]

### Fixed

- **Cámara 3D alineada al haz** — `loadManifest()` y el botón Reset de Combined 3D usan `frameLongestAxisHorizontally()` en lugar de la vista lateral Z hardcodeada: el eje real más largo del haz se enmarca horizontal (cierra la Fase B del plan de visualización)
- **Bragg curve truncada** — `computeBraggCurve()` con `nSteps=1000` × 10 µm solo cubría 1 cm; curvas para E ≥ ~30 MeV se cortaban antes del pico. Default elevado a 100 000 pasos (1 m). Alcance @ 50 MeV: 2.222 cm vs PSTAR 2.227 cm (0.2 %)
- **Partícula congelada en Bragg** — cuando dE/dx se clampea a 0 (fuera de validez de Bethe-Bloch) el loop de integración terminaba por agotamiento de pasos rellenando con ceros; ahora corta tratando la partícula como detenida
- **Energía negativa gana energía** — `computeStep()` con `kinE < 0` retornaba `energyLoss = kinE` (negativo); ahora se clampea a `[0, max(kinE, 0)]`
- **Referencias NIST PSTAR erróneas** — la tabla de validación usaba valores de filas equivocadas (p. ej. 3.37 MeV/cm "a 150 MeV" es el valor de ~300 MeV); corregida con datos reales de PSTAR (agua líquida, matno 276). El motor desvía 0.1–0.7 % de NIST en 10–500 MeV — el "57 % de error" documentado era un artefacto de la referencia mal copiada
- **ProfileManager con JSON corrupto** — `parse(..., allow_exceptions=false)` retorna un valor `discarded` que se propagaba por el merge de configuración y reventaba con `type_error.305`; ahora se detecta `is_discarded()` y se retorna objeto vacío con log de error
- **Portabilidad macOS** — `std::min(uint64_t, size_t)` en `InMemoryStorage::getBatch()` no compila en Darwin (tipos distintos); cast explícito

### Added

- **UI rediseñada** — navegación por riel lateral de 6 secciones (Overview, Scene 3D, Plots, Data, BioSim, Info) en lugar de 14 pestañas; paleta de comandos ⌘K con búsqueda difusa (`ActionRegistry`); QSS modernizado sobre la paleta obsidian existente
- **BeamLabStudio.app + DMG (macOS arm64)** — bundle autocontenido con frameworks Qt embebidos (`scripts/package_macos.sh`), icono `.icns` generado desde el SVG de marca (`scripts/make_icns.sh`), Info.plist
- `tests/unit/domain/test_SimulationEngine.cpp` y `tests/unit/services/test_ProfileManager.cpp` conectados al build (51 → 91 tests)
- `ProfileManager.cpp` compilado en `beamlab_core` (antes solo accesible vía bindings Python)

## [3.0.0] - 2026-05-22

### Added

- **Plugin architecture** — dynamic `.so`/`.dll` loading via `PluginHost` with `create_importer`, `create_exporter`, `create_plugin` symbols
- **EventBus** — type-safe pub/sub with `std::type_index` type erasure, thread-safe via `std::shared_mutex`
- **CommandBus** — CQRS-style command dispatch with `std::type_index` routing
- **MaterialRegistry** — O(1) lookup via `unordered_map`, 55+ ICRU/NIST materials, built-in vs custom separation, JSON serialization
- **ParticleRegistry** — dual index (name + PDG code), 23 PDG 2022 species, O(1) by code
- **SimulationEngine** — unified facade for Bethe-Bloch + Sternheimer + MCS + straggling + Bragg curves, NIST validation ±2%
- **AnalysisOrchestrator** — native C++ pipeline replacing bash/QProcess, multi-engine orchestration with progress/ETA
- **JobScheduler** — topological level execution, ThreadPool-backed, support for `requiresBinnedData` dependencies
- **IAnalysisEngine** — plugin interface for analysis engines with `execute(storage, config, progress)` contract
- **QueryCache** — LRU + TTL for SQLite query results, msgpack serialization, `invalidateByPrefix`, `shared_mutex` thread safety
- **MemoryArena** — O(1) sequential allocator for batch processing, 16 MiB default, alignment support for SIMD
- **ThreadPool** — N-worker pool with `std::future<T>` returns, exception propagation, `activeThreads()` monitoring
- **FrameStatisticsPlugin** — streaming two-pass batch engine (scan → bin), O(bins) memory, supports cancellation
- **Scene3DWidget** — double buffering (`QPixmap`), frustum culling (sphere-plane test), LOD (stride 1/2/8), `invalidateCache()`
- **DockManager** — `IDockableWidget` interface, auto-generated View menu, `QSettings` layout persistence
- **Theme system** — single `beamlabstudio.qss` global stylesheet, no inline `setStyleSheet()`
- **Python API** — pybind11 module exposing MaterialRegistry, ParticleRegistry, SimulationEngine, ProfileManager
- **Profile system** — multi-layer merge: `default.json → profile.json → user.json → CLI overrides`
- **ExporterRegistry** — `CsvExporter`, `ObjExporter`, `ParquetExporter` (CSV-interim), multi-format `exportAll()`
- **ImporterRegistry** — `Geant4CsvImporter`, `ComsolCsvImporter`, `DelimitedTableImporter`, auto-detection by score
- **AnalysisView, BioSimView, ExportView** — compound views with `QSplitter` layouts, presenter dependency injection
- **60+ unit tests**, 15 integration tests, 20+ performance benchmarks
- **CI workflow** — matrix `gcc-13`/`clang-18` × Debug/Release, linting, coverage, benchmarks
- **Release workflow** — auto-build for Linux/macOS/Windows on `v*` tag, changelog generation

### Changed

- **MainWindow** — refactored from 3,157 lines to 198 lines (−94%) by extracting Views, Presenters, DockManager
- **Storage architecture** — replaced monolithic `ISampleStorage` with `IStorageBackend` interface + `SqliteBackend` + `StorageManager`
- **Domain layer** — migrated `src/biosim/` to `src/domain/` with namespace `beamlab::domain::*`, removed `Bio*` prefix
- **Physics** — consolidated `StoppingPowerEngine`, `EnergyStraggling`, `BraggPeakCalculator` under `SimulationEngine` facade
- **Presenters** — `AnalysisPresenter`, `BioSimPresenter`, `ExportPresenter` bridge Qt views with C++ services via `QMetaObject::invokeMethod(QueuedConnection)`
- **Build system** — 12 CMake targets organized by layer, `add_subdirectory(src/ui)` for Qt, `BEAMLAB_ENABLE_PYTHON` for pybind11
- **Architecture docs** — consolidated `ARCHITECTURE.md` with 14 ADRs, layer diagram, threading model, build options

### Fixed

- **B-01** — `finalizeAccumulator()` division by zero: early return on `count==0` with `valid=false` flag ([`FrameStatisticsEngine.cpp`](src/analysis/statistics/FrameStatisticsEngine.cpp))
- **B-02** — MP4 export race condition: replaced `QApplication::processEvents()` with `QEventLoop::ExcludeUserInputEvents`, RAII UI guard ([`MainWindow.cpp`](src/ui/qt/MainWindow.cpp))
- **B-03** — JSON manifest without schema validation: added `REQUIRED_KEYS` check with `showErrorDialog` ([`MainWindow.cpp`](src/ui/qt/MainWindow.cpp))
- **S-06** — Windows script injection: sanitized `%*` wildcard in `run_beamlab_full.cmd`

### Security

- **S-01** — Removed `push.txt` with exposed credentials from git history
- **S-03** — Path traversal validation in `copyDirectoryRecursive()`
- **S-05** — Unpredictable temp file names replaced pattern `.beamlab_frames_*`

---

## [0.2.0] - 2026-05-10

### Added
- BioSim module — biological simulation with Bethe-Bloch + Sternheimer + MCS
- Synthetic dataset generator (6 test cases: TC01–TC06)
- `physics_validation.py` — NIST PSTAR cross-validation script
- FrameStatistics, FocusDetector, SurfaceBuilder engines
- SQLite streaming import (O(1) RAM for GB-size CSVs)
- Basic Qt6 UI with 3D viewport (QPainter)
- 41 unit tests

### Changed
- Rewrote MuonSimViewer from Python prototype to C++/Qt

---

## [0.1.0] - 2026-03-15

### Added
- Initial prototype: MuonSimViewer in Python/Matplotlib
- Geant4 CSV import pipeline
- Bash-based analysis workflow
