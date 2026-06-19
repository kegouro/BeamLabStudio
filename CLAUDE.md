# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build

```bash
# Debug (headless — no Qt)
cmake -B build && cmake --build build -j$(nproc)

# Debug con UI Qt6
cmake -B build -DBEAMLAB_ENABLE_QT_UI=ON && cmake --build build -j$(nproc)

# Release headless
cmake -B build-release -DCMAKE_BUILD_TYPE=Release && cmake --build build-release -j$(nproc)
```

Lanzar la UI (auto-recompila si los fuentes son más nuevos que el binario):
```bash
./launch_beamlabstudio.sh
```

CLI:
```bash
./build/beamlab -i <archivo.csv> -o <directorio_salida>
./build/beamlab -i data.csv -o /tmp/out --preview-trajectories 10000
```

## Tests

```bash
# Tests unitarios (los únicos compilados actualmente)
ctest --test-dir build --output-on-failure -L unit

# Un test específico
./build/beamlab_tests --gtest_filter=FrameStatisticsEngineTest.*
./build/beamlab_tests --gtest_filter=PhysicsRegressionTest.MCSFormula
```

Los tests unitarios activos están listados explícitamente en `tests/unit/CMakeLists.txt` — el ejecutable es `build/beamlab_tests`. Los subdirectorios `tests/unit/domain/`, `tests/unit/platform/` y `tests/unit/services/` contienen tests aún no conectados al build. Los tests de integración (`tests/integration/`) y benchmarks (`tests/performance/`) existen pero tampoco están en `tests/CMakeLists.txt` todavía.

Datos de referencia para regresiones en `tests/synthetic_dataset/TC01_normal_beam/` … `TC06_missing_files/` — cada caso incluye CSV de entrada y salidas esperadas (focus_curve, envelope_rings, OBJ).

## Arquitectura

El punto de entrada unificado para CLI y UI es `ApplicationBootstrap::run()` (`src/app/ApplicationBootstrap.cpp`). La UI lo llama desde un worker thread en `AnalysisPresenter::runOnThread()` — no usa la clase `AnalysisPipeline` (el miembro `pipeline_` en `AnalysisPresenter` queda sin usar).

### Capas (de arriba hacia abajo, sin dependencias inversas)

```
beamlab_ui        Qt6 — MainWindow, Presenters (MVP), widgets
    ↓
beamlab_biosim    Simulación biológica — física (Bethe-Bloch, Bragg, straggling),
                  materiales, geometría phantom, widgets Qt propios  [solo con BEAMLAB_ENABLE_QT_UI]
    ↓
beamlab_core      ApplicationBootstrap, motores de análisis, importadores legacy (src/io/),
                  SqliteStorage/InMemoryStorage, ConfigLoader, Vec3/math
    ↓
beamlab_domain    Física médica, materiales, geometría — sin Qt, sin singletons
                  (SimulationEngine es la fachada unificada de Bethe-Bloch/MCS/Bragg)
```

Infraestructura transversal (no es un target compilado, solo cabeceras en `src/platform/`):
`EventBus`, `CommandBus`, `IPlugin`/`PluginHost` — bus de eventos y sistema de plugins `.so`.

### Modelos de datos canónicos (`src/data/`)

Las estructuras que fluyen entre capas viven en `src/data/model/`:
`TrajectoryDataset` → contiene `Trajectory` → contiene `TrajectorySample` (posición + energía + step).
Los IDs tipados (`SampleId`, `TrajectoryId`, `DatasetId`) evitan confusión entre índices.
`src/data/repositories/ITrajectoryRepository.h` define la interfaz de repositorio que `SqliteTrajectoryRepository` implementa.

### Flujo de análisis

1. `Geant4CsvImporter` (`src/io/importers/`) → `TrajectoryDataset` en `SqliteStorage` (o `InMemoryStorage` para archivos pequeños)
2. `FrameStatisticsEngine::compute()` → 501 bins axiales (default) con r_rms por bin
3. `FocusDetector::detect()` → curva de foco (índice del bin con r_rms mínimo)
4. `VisualizationDataExporter` → CSV de anillos de envelope para el viewport 3D

### Migración en progreso

`src/io/` e `src/core/storage/` son el código **activo** hoy (compilado en `beamlab_core`).
`src/services/` es la arquitectura destino — tiene sus propias versiones de importadores (`services/import/`), exportadores (`services/export/`) y storage (`services/storage/`) con interfaces limpias (`IImporter`, `IExporter`, `IStorageBackend`). `AnalysisOrchestrator` (`services/analysis/`) es el reemplazo futuro de `ApplicationBootstrap`. Aún no se usa en producción.

### Presenters (MVP)

Los Presenters (`src/ui/qt/presenters/`) son el único código Qt que toca lógica de negocio. Las Views no conocen el dominio. Los callbacks de worker threads se marshallan al hilo Qt con `QMetaObject::invokeMethod(..., Qt::QueuedConnection)`.

### Storage backends

- `InMemoryStorage`: archivos pequeños, todo en RAM
- `SqliteStorage`: archivos grandes, base de datos en disco (no `:memory:`) — límite de ~1 GB RAM para conjuntos de 20 GB

## Opciones de CMake relevantes

| Flag | Default | Descripción |
|------|---------|-------------|
| `BEAMLAB_ENABLE_QT_UI` | OFF | Compila la UI Qt6 y beamlab_biosim |
| `BEAMLAB_ENABLE_ROOT` | OFF | Importador nativo CERN ROOT (.root) |
| `BEAMLAB_ENABLE_PARQUET` | OFF | Exportador Apache Parquet (requiere Arrow) |
| `BEAMLAB_ENABLE_PERFORMANCE_TESTS` | ON | Benchmarks |
| `BEAMLAB_ENABLE_LTO` | ON | Link Time Optimization (solo Release) |
| `BEAMLAB_ENABLE_NATIVE_ARCH` | OFF | `-march=native` |
| `BEAMLAB_BUILD_PLUGIN_TEMPLATES` | OFF | Compila .so de ejemplo en src/plugins/ |

## Acuerdo de trabajo (actualizado 2026-06-19)

Reconstrucción en curso según `docs/PLAN_MAESTRO.md` (v2). Las restricciones previas (solo presenters / no tocar UI / no tocar docs) están **levantadas**: se puede modificar todo el árbol. Reglas vigentes:

- Trabajo en `integration/reconstruction`; merge a `main` solo al cierre de F6.
- Migrar y verificar **antes** de borrar (no borrar `src/io` ni `BioSimWidget` sin su fase verificada).
- Toda constante física con cita NIST/ICRU/PDG; sin números mágicos.
- Estado por fase en `.claude/PHASE_STATE.md`.

## Convenciones de código (C++20)

- **Recursos:** RAII; `std::unique_ptr`/stack; `shared_ptr` solo con ownership compartido real; `std::span<const T>` para vistas no propietarias.
- **const/noexcept:** `const` en métodos que no mutan; `noexcept` en moves/dtors/leaf; `[[nodiscard]]` en factories/validators/compute.
- **Errores:** `std::optional` + código de error para fallos esperables; excepciones para lo inesperado; nunca `throw` en dtor. **`std::expected` NO** (C++23; el proyecto es C++20). `tl::expected` solo si surge necesidad real (dependencia justificada).
- **Templates:** concepts C++20 donde aporten (`template<std::floating_point T>`), no SFINAE; sin generalizar por especular (YAGNI).
- **Constantes físicas:** `constexpr` con cita — `constexpr double proton_mass_MeV = 938.27208816; // PDG 2022`.
- **Naming:** `snake_case` vars/funciones, `PascalCase` tipos, `CONSTANT_CASE` constexpr globales, miembro privado sufijo `_`.
- **Tests:** `test_<unidad>_<escenario>_<esperado>`; Arrange-Act-Assert.
- **CMake:** target-based (`target_*`); `PRIVATE/PUBLIC/INTERFACE` correcto; `-Werror` solo tras limpiar warnings (F5).
- **Qt6:** `connect` con functor (`&Clase::señal`), nunca macros `SIGNAL/SLOT`; `QPointer` para ownership no exclusiva; captura mínima en lambdas.
- **Git:** Conventional Commits (`feat(physics):`, `fix(ui):`, `refactor(services):`); un commit por unidad lógica.
- **Ponytail:** preferir borrar a añadir; atajos deliberados marcados `// ponytail: <qué> | techo: <límite> | upgrade: <cómo>`.
