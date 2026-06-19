# BLUEPRINT — BeamLabStudio v3.0 "Antares"

> **Fecha:** 2026-05-22
> **Arquitecto:** DeepSeek V4 Pro via OpenCode
> **Fuentes:** PRA_Auditoria_Tecnica.md (score 64/100) + Knowledge Graph (1696 nodes, 2375 edges, 267 communities) + NEW_ARCHITECTURE.md + análisis de código vivo
> **Estado:** Propuesta de re-arquitectura completa — pendiente de aprobación

---

## Resumen Ejecutivo

BeamLabStudio es funcional pero frágil. Con 3,157 líneas en `MainWindow.cpp`, cohesión de 0.06 en su comunidad central, 237 nodos débilmente conectados en el grafo de conocimiento, y un pipeline que depende de `QProcess` + bash + Python, la aplicación no escalará a los casos de uso que vienen: CSVs de 20 GB, procesamiento batch en servidores, colaboración multi-investigador, y publicación de resultados.

Este blueprint transforma BeamLabStudio de una app Qt monolítica a una **plataforma científica modular** con tres modos de operación: Desktop (Qt6), Headless (CLI/server), y Embedded (Python API). No es un refactor — es una refundación sobre los cimientos existentes, preservando toda la física validada y los 41 tests que ya pasan.

### Métricas objetivo

| Métrica | Actual (v0.2.0) | Objetivo (v3.0) |
|---------|-----------------|-----------------|
| `MainWindow.cpp` líneas | 3,157 | <400 |
| RAM máxima CSV 20 GB | OOM (>30 GB) | <1 GB |
| Tiempo import 1 GB CSV | 37s | <15s |
| Cobertura tests | <40% | >85% (core), >70% (total) |
| Comunidades con cohesión <0.10 | 4 (MainWindow, BioMaterial, Analysis Config, ConvexHull) | 0 |
| Nodos débilmente conectados | 237 | <20 |
| Bugs críticos abiertos | 6 | 0 |
| Formato de entrada soportado | CSV, TSV, ROOT | Plugin-based: cualquier formato |
| UI responsiveness durante análisis | Se congela | Totalmente asíncrono |
| Modos de operación | Desktop GUI + CLI | Desktop + Headless Server + Python API |
| Pipeline de análisis | Bash + Python + QProcess | Nativo C++ con progreso real |

### Esfuerzo total estimado: ~120 horas

---

## 1. Diagnóstico: Lo que el Knowledge Graph reveló

El grafo de conocimiento generado a partir de 277 archivos (131K palabras) expuso patrones que el código solo no muestra:

### 1.1 Comunidades con cohesión crítica

| Comunidad | Cohesión | Diagnóstico |
|-----------|----------|-------------|
| **MainWindow UI** (76 nodos) | **0.06** | God Object confirmado. Las funciones no se llaman entre sí — todas convergen en `buildUi()` como único punto de acoplamiento. |
| **Analysis Configuration** (46 nodos) | **0.04** | Parámetros dispersos sin relación estructural. Cada flag de config es una isla. |
| **Convex Hull Envelope** (47 nodos) | **0.08** | Funciones utilitarias sin orquestador claro. Alta cohesión interna de datos pero sin fachada. |
| **BioMaterial Library** (62 nodos) | **0.06** | 55 materiales como nodos independientes. Solo `addCustom()` y `findById()` los conectan. Necesita un registry pattern. |

### 1.2 God Nodes (nodos hiper-conectados)

| Nodo | Grado | Problema |
|------|-------|----------|
| `BioMaterialLibrary()` | 55 | Demasiadas dependencias entrantes. Cada módulo que usa materiales depende directamente de esta clase. |
| `buildUi()` | 21 | Toda la UI se construye desde un solo método. Imposible testear individualmente. |
| `ParticleLibrary()` | 18 | Similar al problema de materiales. |
| `Geant4Columns` | 17 | Acoplamiento fuerte con el formato específico de Geant4. |

### 1.3 Conexiones sorprendentes (inferred, no documentadas)

- **OOM con TrajectoryDataset (96M samples)** ↔ **Big Data Streaming (O(1) RAM)**: La solución de streaming ya existe en los docs pero no está implementada completamente.
- **PRA Fase 0–2** ↔ **Plan de Ataque Fase 1–3**: Las fases de ambos documentos son semánticamente equivalentes pero con nombres diferentes. Señal de documentación desincronizada.
- **Streaming-First Principle** ↔ **Big Data Streaming**: El principio está declarado en NEW_ARCHITECTURE.md pero el código aún tiene `TrajectoryDataset::trajectories` como vector público.

### 1.4 Hypergrafos identificados

1. **Strategy Pattern para Sample Storage** — `ISampleStorage → InMemoryStorage / SqliteStorage` [EXTRACTED]
2. **BioSim Unified Physics Engine** — `StoppingPowerEngine + EnergyStraggling + BraggPeakCalculator + BioMaterialValidator` [EXTRACTED]
3. **Audit → Plan → Architecture → PRA Pipeline** — Los 4 documentos forman un circuito de retroalimentación [EXTRACTED]
4. **Synthetic Dataset Validation Suite** — 6 TC + resultados esperados [EXTRACTED]
5. **Unit Test Execution Pipeline** — CMake → GTest → CTest → CI [INFERRED]

---

## 2. Principios de Diseño (7 Pilares)

| # | Principio | Significado concreto |
|---|-----------|---------------------|
| **P1** | **Event-Driven Core** | Un `EventBus` central reemplaza todas las dependencias directas entre módulos. Los módulos publican eventos, otros se suscriben. Nadie conoce a nadie. |
| **P2** | **Plugin Architecture** | Importers, exporters, visualizers y análisis engines son plugins cargables dinámicamente. El core no conoce formatos específicos. |
| **P3** | **Streaming-First, Always** | Toda operación sobre datasets es O(1) en RAM. No existe el concepto de "cargar todo en memoria". |
| **P4** | **Zero Hardcode** | Cada parámetro (físico, numérico, UI) vive en archivos de perfil. Override por usuario, por proyecto, por CLI. |
| **P5** | **Async by Default** | Pipeline de análisis, importación y exportación corren en threads dedicados. La UI nunca se bloquea. |
| **P6** | **Testable by Construction** | Dependency injection en todos los constructores. Mocks para storage, eventos y config. Sin singletons. |
| **P7** | **Observable Everything** | Cada paso del pipeline emite progreso con bytes, ETA y tasa. Telemetría opcional para debugging de performance. |

---

## 3. Arquitectura Objetivo — Visión General

### 3.1 Diagrama de Capas

```
┌──────────────────────────────────────────────────────────────┐
│                     APPLICATION LAYER                          │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────────┐  │
│  │ Qt6 GUI  │  │ CLI Mode │  │ HTTP API │  │  Python API  │  │
│  │ (Desktop)│  │ (Server) │  │ (Future) │  │  (pybind11)  │  │
│  └────┬─────┘  └────┬─────┘  └────┬─────┘  └──────┬───────┘  │
│       └──────────────┴────────────┴────────────────┘          │
│                          │                                     │
├──────────────────────────┼────────────────────────────────────┤
│                    COMMAND BUS (CQRS)                          │
│  ┌──────────────────────────────────────────────────────┐     │
│  │  Commands: ImportFile, RunAnalysis, ExportResults    │     │
│  │  Queries:  GetStats, GetFocus, GetEnvelope          │     │
│  │  Events:   ProgressUpdated, AnalysisCompleted        │     │
│  └──────────────────────┬───────────────────────────────┘     │
│                          │                                     │
├──────────────────────────┼────────────────────────────────────┤
│                    PLUGIN HOST LAYER                           │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐      │
│  │ Importer │  │ Exporter │  │ Analysis │  │   Viz    │      │
│  │ Registry │  │ Registry │  │  Engine  │  │  Engine  │      │
│  │          │  │          │  │ Registry │  │ Registry │      │
│  └────┬─────┘  └────┬─────┘  └────┬─────┘  └────┬─────┘      │
│       │              │             │             │             │
├───────┼──────────────┼─────────────┼─────────────┼─────────────┤
│       │         CORE SERVICES LAYER               │             │
│  ┌────┴──────────────────────────────────────┬────┘             │
│  │  StorageManager  ConfigManager  JobScheduler│                │
│  │  EventBus        LogService     Telemetry   │                │
│  └──────────────────────┬──────────────────────┘                │
│                          │                                     │
├──────────────────────────┼────────────────────────────────────┤
│                    DOMAIN LAYER                                 │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐      │
│  │ Physics  │  │ Materials│  │Geometry  │  │  Math    │      │
│  │ Engine   │  │ Registry │  │  Engine  │  │ Library  │      │
│  └──────────┘  └──────────┘  └──────────┘  └──────────┘      │
│                                                                 │
├────────────────────────────────────────────────────────────────┤
│                    INFRASTRUCTURE LAYER                         │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐      │
│  │ SQLite   │  │ File I/O │  │Threading │  │Memory    │      │
│  │ Backend  │  │ (mmap)   │  │ (TPool)  │  │(Arena)   │      │
│  └──────────┘  └──────────┘  └──────────┘  └──────────┘      │
└────────────────────────────────────────────────────────────────┘
```

### 3.2 Estructura de Directorios Objetivo

```
src/
├── app/                              # Entry points (mínimo)
│   ├── main_desktop.cpp              # Qt6 GUI entry
│   ├── main_cli.cpp                  # CLI/headless entry
│   ├── main_server.cpp               # HTTP server entry (futuro)
│   └── ApplicationBootstrap.h/.cpp   # Wiring universal
│
├── platform/                         # ✨ NUEVO: Application host
│   ├── CommandBus.h/.cpp             # CQRS command/query dispatcher
│   ├── EventBus.h/.cpp               # Pub/sub event system
│   ├── PluginHost.h/.cpp             # Dynamic library loader
│   ├── ServiceRegistry.h/.cpp        # DI container (existente, mejorar)
│   └── ApplicationContext.h/.cpp      # Contexto global (existente)
│
├── core/                             # Fundaciones (sin cambios mayores)
│   ├── math/                         # Vec2, Vec3, Mat3, AABB, Plane3D, Basis3D
│   ├── foundation/                   # Result<T>, Error, Warning, NumericGuards
│   ├── units/                        # Constantes físicas, BeamGeometry
│   └── config/                       # ConfigLoader, AnalysisConfig, ProfileManager
│       ├── ConfigLoader.h/.cpp       # (existente, expandir)
│       ├── AnalysisConfig.h          # (existente, refinar)
│       └── ProfileManager.h/.cpp     # ✨ NUEVO: multi-perfil
│
├── domain/                           # ✨ RENOMBRADO: lógica de negocio pura
│   ├── physics/                      # ← ex-biosim/physics/
│   │   ├── StoppingPowerEngine.h/.cpp
│   │   ├── BraggPeakCalculator.h/.cpp
│   │   ├── EnergyStraggling.h/.cpp
│   │   └── ParticleLibrary.h/.cpp
│   ├── materials/                    # ← ex-biosim/materials/
│   │   ├── Material.h                # ✨ Renombrado: BioMaterial → Material
│   │   ├── MaterialRegistry.h/.cpp   # ✨ NUEVO: registry pattern
│   │   └── MaterialValidator.h/.cpp
│   ├── geometry/                     # ← ex-biosim/geometry/
│   │   ├── Slab.h                    # ✨ Renombrado: BioSlab → Slab
│   │   ├── PhantomLibrary.h/.cpp
│   │   └── PhantomPreset.h
│   └── simulation/                   # ← ex-biosim/core/
│       ├── BioSimRunner.h/.cpp
│       ├── BioSimConfig.h
│       └── BioSimResult.h
│
├── data/                             # Modelos (sin cambios mayores)
│   ├── model/                        # Trajectory, Sample, Dataset, etc.
│   ├── ids/                          # SampleId, TrajectoryId, DatasetId
│   └── repositories/                 # ✨ NUEVO: data access objects
│       ├── ITrajectoryRepository.h
│       └── SqliteTrajectoryRepository.h/.cpp
│
├── services/                         # ✨ NUEVO: capa de servicios
│   ├── storage/                      # ← migrado de core/storage/
│   │   ├── IStorageBackend.h         # Interfaz unificada
│   │   ├── InMemoryBackend.h/.cpp
│   │   ├── SqliteBackend.h/.cpp
│   │   └── StorageManager.h/.cpp     # ✨ NUEVO: orquestador
│   ├── import/                       # ← ex-io/importers/
│   │   ├── IImporter.h               # Interfaz plugin
│   │   ├── ImporterRegistry.h/.cpp   # ✨ NUEVO: plugin registry
│   │   ├── Geant4CsvImporter.h/.cpp
│   │   ├── ComsolCsvImporter.h/.cpp
│   │   ├── RootNativeImporter.h/.cpp
│   │   └── DelimitedTableImporter.h/.cpp
│   ├── export/                       # ← ex-io/exporters/
│   │   ├── IExporter.h
│   │   ├── ExporterRegistry.h/.cpp   # ✨ NUEVO: plugin registry
│   │   ├── ObjExporter.h/.cpp
│   │   ├── PngExporter.h/.cpp
│   │   ├── Mp4Exporter.h/.cpp
│   │   ├── PdfExporter.h/.cpp
│   │   └── CsvExporter.h/.cpp
│   ├── analysis/                     # ← ex-analysis/ + core/pipeline/
│   │   ├── engines/                  # ✨ NUEVO: análisis como plugins
│   │   │   ├── IAnalysisEngine.h     # Interfaz plugin
│   │   │   ├── FrameStatisticsEngine.h/.cpp
│   │   │   ├── BatchStatisticsEngine.h/.cpp
│   │   │   ├── FocusDetector.h/.cpp
│   │   │   ├── EnvelopeExtractor.h/.cpp
│   │   │   ├── SurfaceBuilder.h/.cpp
│   │   │   └── QualityChecker.h/.cpp
│   │   ├── AnalysisOrchestrator.h/.cpp  # Coordina engines
│   │   ├── JobScheduler.h/.cpp          # ✨ NUEVO: thread pool
│   │   └── ProgressTracker.h/.cpp
│   └── io/                           # ← ex-io/ (parsing, normalization)
│       ├── parsing/                  # DelimiterDetector, HeaderAnalyzer
│       ├── normalization/            # DatasetNormalizer, AxisResolver
│       └── manifest/                 # ManifestReader, ManifestWriter
│
├── ui/                               # UI (solo Vista)
│   ├── qt/
│   │   ├── MainWindow.h/.cpp         # <400 líneas objetivo
│   │   ├── presenters/
│   │   │   ├── AnalysisPresenter.h/.cpp
│   │   │   ├── ExportPresenter.h/.cpp
│   │   │   └── BioSimPresenter.h/.cpp    # ✨ NUEVO
│   │   ├── widgets/
│   │   │   ├── Scene3DWidget.h/.cpp
│   │   │   ├── ObjViewerWidget.h/.cpp
│   │   │   ├── StatsDashboardWidget.h/.cpp
│   │   │   ├── RunDashboardWidget.h/.cpp
│   │   │   ├── BioSimWidget.h/.cpp
│   │   │   ├── BioViewport3D.h/.cpp
│   │   │   ├── SlabEditor3D.h/.cpp
│   │   │   ├── MaterialEditorDialog.h/.cpp
│   │   │   └── EnergyScaleBar.h/.cpp
│   │   ├── dockable/                  # ✨ NUEVO: widgets dockeables
│   │   │   ├── IDockableWidget.h
│   │   │   └── DockManager.h/.cpp
│   │   ├── styles/
│   │   │   └── beamlabstudio.qss
│   │   └── assets/
│   │       └── branding/
│   └── views/                         # ✨ NUEVO: vistas compuestas
│       ├── AnalysisView.h/.cpp
│       ├── BioSimView.h/.cpp
│       └── ExportView.h/.cpp
│
├── plugins/                           # ✨ NUEVO: plugins externos
│   ├── importers/
│   │   └── template/                  # Template para nuevos importers
│   ├── exporters/
│   │   └── template/
│   └── engines/
│       └── template/
│
├── scripting/                         # ✨ NUEVO: Python bindings
│   ├── pybind11/                      # (FetchContent)
│   ├── beamlab_module.cpp             # Módulo Python
│   └── examples/                      # Jupyter notebooks
│
tests/
├── unit/                              # GoogleTest (expandir 7→40+)
│   ├── domain/                        # Physics, materials, geometry
│   ├── services/                      # Storage, import, export, analysis
│   ├── platform/                      # CommandBus, EventBus, PluginHost
│   └── data/
├── integration/                       # ✨ NUEVO: end-to-end
│   ├── test_import_pipeline.cpp
│   ├── test_analysis_pipeline.cpp
│   ├── test_export_pipeline.cpp
│   └── test_biosim_pipeline.cpp
├── performance/                       # ✨ NUEVO: benchmarks
│   ├── bench_csv_import.cpp
│   ├── bench_sqlite_batch.cpp
│   ├── bench_statistics_engine.cpp
│   └── bench_scene3d_render.cpp
├── regression/                        # ✨ NUEVO: golden files
│   └── expected/                      # Outputs de referencia
└── data/                              # Fixtures (existente)
    ├── test_geant4_stepping.csv
    └── synthetic/                     # Datasets sintéticos

config/
├── default_analysis.json              # (existente)
├── profiles/                          # ✨ NUEVO
│   ├── quick_inspect.json             # Perfil rápido (pocos bins, preview)
│   ├── full_analysis.json             # Perfil completo (alta resolución)
│   ├── clinical_dosimetry.json        # Perfil para uso médico
│   └── batch_server.json              # Perfil para headless server
└── plugins.json                       # ✨ NUEVO: registro de plugins

docs/
├── BLUEPRINT.md                       # Este documento
├── ARCHITECTURE.md                    # ✨ NUEVO: doc vivo de arquitectura
├── CONTRIBUTING.md                    # ✨ NUEVO: guía de contribución
├── PLUGIN_DEVELOPMENT.md              # ✨ NUEVO: cómo escribir plugins
└── API_REFERENCE.md                   # ✨ NUEVO: referencia de API
```

---

## 4. Fases de Implementación

Cada fase incluye:
- **Objetivo**: qué se logra al final
- **Duración**: estimado en horas
- **Archivos afectados**: paths concretos
- **Pasos**: instrucciones de implementación secuenciales
- **Validación**: cómo verificar que la fase está completa
- **Checklist**: items marcables para seguimiento

---

### FASE 0: Safety Stop (Parada de Seguridad)

**Objetivo:** Eliminar vulnerabilidades críticas y bugs que pueden causar pérdida de datos o crashes en producción. Esta fase es PREVIA a cualquier refactor — no se toca arquitectura hasta que el paciente esté estable.

**Duración:** 6 horas
**Prioridad:** INMEDIATA — hacer antes que cualquier otra cosa

---

#### Paso 0.1: Revocar credenciales expuestas

**Archivo:** `push.txt` (raíz del repo)
**Riesgo:** Token de GitHub potencialmente activo en historial git

**Implementación:**
```bash
# 1. Verificar si el token es real
git log --all --full-history -- push.txt

# 2. Si es real, revocar en GitHub:
#    GitHub Settings → Developer settings → Personal access tokens → Revocar

# 3. Eliminar del historial
git filter-branch --force --index-filter \
  "git rm --cached --ignore-unmatch push.txt" \
  --prune-empty --tag-name-filter cat -- --all

# 4. Agregar a .gitignore
echo "push.txt" >> .gitignore
git add .gitignore
git commit -m "security: remove push.txt with credentials from history"

# 5. Forzar push (con precaución)
# git push origin --force --all
```

---

#### Paso 0.2: Fix race condition en exportación MP4

**Archivo:** `src/ui/qt/MainWindow.cpp:3023`
**Bug ID:** B-02
**Riesgo:** Video export corrupto si usuario interactúa con UI durante captura

**Implementación:**
```cpp
// En MainWindow::exportTrajectoryVideoTo(), reemplazar TODA llamada a:
QApplication::processEvents();

// Por este bloque:
{
    QEventLoop loop;
    loop.processEvents(QEventLoop::ExcludeUserInputEvents);
}

// ADEMÁS, al inicio de la exportación:
tabs_->setEnabled(false);
combined_controls_dock_->setEnabled(false);

// Y al final (usar scope guard RAII):
struct UIRestoreGuard {
    QTabWidget* tabs;
    QDockWidget* dock;
    ~UIRestoreGuard() {
        tabs->setEnabled(true);
        dock->setEnabled(true);
    }
};
UIRestoreGuard guard{tabs_, combined_controls_dock_};
```

---

#### Paso 0.3: Fix división por cero en finalizeAccumulator

**Archivo:** `src/analysis/statistics/FrameStatisticsEngine.cpp:96`
**Bug ID:** B-01
**Riesgo:** NaN en cascada corrompe todos los stats downstream

**Implementación:**
```cpp
// En namespace anónimo, función finalizeAccumulator():
FrameStatistics finalizeAccumulator(const FrameAccumulator& acc) {
    FrameStatistics stats{};   // Ya existe, inicializado a cero
    if (acc.count == 0) {
        stats.valid = false;   // ✨ AÑADIR: flag de validez
        return stats;           // ← AÑADIR ESTA LÍNEA (early return)
    }
    const double inv_n = 1.0 / static_cast<double>(acc.count);
    // ... resto del código sin cambios
}
```

---

#### Paso 0.4: Validación de schema en loadManifest

**Archivo:** `src/ui/qt/MainWindow.cpp:2397`
**Bug ID:** B-03
**Riesgo:** Crash con JSON malformado de fuentes externas

**Implementación:**
```cpp
// En MainWindow::loadManifest():
QJsonDocument doc = QJsonDocument::fromJson(data);
if (doc.isNull() || !doc.isObject()) {
    showErrorDialog("Invalid manifest JSON",
                    "The file could not be parsed as JSON.");
    return false;
}

// ✨ AÑADIR validación de schema:
QJsonObject root = doc.object();
static const QStringList REQUIRED_KEYS = {
    "dataset_id", "input_file", "analysis_config"
};
for (const auto& key : REQUIRED_KEYS) {
    if (!root.contains(key)) {
        showErrorDialog("Invalid manifest schema",
            QString("Missing required key: '%1'").arg(key));
        return false;
    }
}
```

---

#### Paso 0.5: Sanitizar scripts de Windows

**Archivo:** `scripts/run_beamlab_full.cmd:4`
**Bug ID:** S-06
**Riesgo:** Inyección de comandos vía nombres de archivo con caracteres especiales

**Implementación:**
```cmd
:: Reemplazar:
:: beamlab.exe %*

:: Por:
:: beamlab.exe %1 %2 %3 %4 %5 %6 %7 %8 %9
```

---

#### Checklist Fase 0

- [ ] **S-01** Token GitHub verificado y revocado. `push.txt` no existe en HEAD ni historial.
- [ ] **B-02** Exportación MP4: 3 exports consecutivos sin corrupción, clickeando UI durante captura.
- [ ] **B-01** `finalizeAccumulator()` con `count==0` retorna stats con `valid=false`, sin NaN.
- [ ] **B-03** Cargar manifiesto con JSON inválido muestra error, no crash.
- [ ] **S-06** Script `.cmd` no ejecuta comandos arbitrarios con nombres de archivo maliciosos.
- [ ] `ctest` pasa 41/41 sin regresiones.

---

### FASE 1: Foundation — Infraestructura Modular

**Objetivo:** Establecer el núcleo arquitectónico que permite todo lo demás: EventBus, CommandBus, PluginHost, y el nuevo sistema de configuración por perfiles. Sin tocar funcionalidad existente.

**Duración:** 18 horas
**Prioridad:** ALTA — los cimientos deben ser sólidos antes de migrar módulos

---

#### Paso 1.1: EventBus — sistema de publicación/suscripción

**Archivos nuevos:**
- `src/platform/EventBus.h`
- `src/platform/EventBus.cpp`
- `tests/unit/test_EventBus.cpp`

**Implementación:**

```cpp
// src/platform/EventBus.h
#pragma once
#include <functional>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <typeindex>

namespace beamlab::platform {

class EventBus {
public:
    template<typename E>
    using Handler = std::function<void(const E&)>;

    template<typename E>
    SubscriptionId subscribe(Handler<E> handler) {
        auto& handlers = handlers_[std::type_index(typeid(E))];
        // Type-erased wrapper
        handlers.push_back([handler](const void* e) {
            handler(*static_cast<const E*>(e));
        });
        return SubscriptionId{nextId_++};
    }

    template<typename E>
    void publish(const E& event) {
        auto it = handlers_.find(std::type_index(typeid(E)));
        if (it == handlers_.end()) return;
        for (auto& h : it->second) {
            h(static_cast<const void*>(&event));
        }
    }

    void unsubscribe(SubscriptionId id);

private:
    std::unordered_map<std::type_index,
        std::vector<std::function<void(const void*)>>> handlers_;
    std::mutex mutex_;
    uint64_t nextId_ = 1;
};

// Eventos predefinidos:
struct ImportStarted { std::string filePath; uint64_t fileSize; };
struct ImportProgress { uint64_t bytesRead; uint64_t totalBytes; double etaSeconds; };
struct ImportCompleted { DatasetId id; uint64_t totalSamples; };
struct AnalysisStarted { DatasetId id; };
struct AnalysisProgress { int percentComplete; std::string currentStage; };
struct AnalysisCompleted { DatasetId id; std::string outputDir; };
struct ExportStarted { std::string format; };
struct ExportProgress { int fileIndex; int totalFiles; };
struct ExportCompleted { std::string outputDir; };
struct ErrorOccurred { Severity severity; std::string message; std::string source; };

} // namespace beamlab::platform
```

---

#### Paso 1.2: CommandBus — patrón CQRS para operaciones

**Archivos nuevos:**
- `src/platform/CommandBus.h`
- `src/platform/CommandBus.cpp`
- `tests/unit/test_CommandBus.cpp`

**Implementación:**

```cpp
// src/platform/CommandBus.h
// Separa comandos (mutaciones) de queries (lecturas)
// Cada comando tiene exactamente un handler
// Las queries pueden cachearse

template<typename TCommand, typename TResult>
class CommandHandler {
public:
    virtual TResult handle(const TCommand& cmd) = 0;
};

class CommandBus {
public:
    template<typename TCmd, typename TRes>
    void registerHandler(std::unique_ptr<CommandHandler<TCmd, TRes>> handler);

    template<typename TCmd, typename TRes>
    TRes dispatch(const TCmd& cmd);
};

// Comandos predefinidos:
struct ImportFileCommand { std::string path; ProfileName profile; };
struct RunAnalysisCommand { DatasetId datasetId; AnalysisConfig config; };
struct ExportResultsCommand { DatasetId id; std::string format; std::string outDir; };
struct LoadManifestCommand { std::string path; };
```

---

#### Paso 1.3: PluginHost — sistema de plugins dinámicos

**Archivos nuevos:**
- `src/platform/PluginHost.h`
- `src/platform/PluginHost.cpp`
- `src/platform/IPlugin.h`
- `tests/unit/test_PluginHost.cpp`

**Implementación:**

```cpp
// src/platform/IPlugin.h
class IPlugin {
public:
    virtual ~IPlugin() = default;
    virtual std::string name() const = 0;
    virtual std::string version() const = 0;
    virtual std::string description() const = 0;
    virtual void initialize(ApplicationContext& ctx) = 0;
    virtual void shutdown() = 0;
};

// src/platform/PluginHost.h
class PluginHost {
public:
    // Escanea directorios en busca de plugins
    void scanDirectory(const std::string& path);

    // Carga un plugin desde shared library (.so/.dylib/.dll)
    std::unique_ptr<IPlugin> load(const std::string& libPath);

    // Registra un plugin built-in (compilado estáticamente)
    void registerBuiltin(std::unique_ptr<IPlugin> plugin);

    // Inicializa todos los plugins cargados
    void initializeAll(ApplicationContext& ctx);

    // Obtiene todos los plugins de un tipo específico
    template<typename T>
    std::vector<T*> getPluginsOfType();

private:
    std::vector<std::unique_ptr<IPlugin>> plugins_;
    std::vector<void*> loadedLibs_;  // dlopen handles
};

// Registro de tipos de plugin:
enum class PluginType {
    Importer,
    Exporter,
    AnalysisEngine,
    VisualizationEngine,
    MaterialProvider,
};
```

---

#### Paso 1.4: ProfileManager — configuración multi-perfil

**Archivos nuevos:**
- `src/core/config/ProfileManager.h`
- `src/core/config/ProfileManager.cpp`
- `config/profiles/quick_inspect.json`
- `config/profiles/full_analysis.json`
- `config/profiles/clinical_dosimetry.json`
- `config/profiles/batch_server.json`
- `tests/unit/test_ProfileManager.cpp`

**Implementación:**

```cpp
// src/core/config/ProfileManager.h
class ProfileManager {
public:
    // Carga un perfil por nombre (busca en config/profiles/<name>.json)
    AnalysisConfig loadProfile(const std::string& profileName);

    // Lista perfiles disponibles
    std::vector<std::string> availableProfiles() const;

    // Guarda overrides del usuario en ~/.config/BeamLabStudio/
    void saveUserOverrides(const std::string& profileName,
                           const nlohmann::json& overrides);

    // Merge: default → profile → user overrides → CLI args
    AnalysisConfig resolve(const std::string& profileName,
                           const nlohmann::json& cliOverrides = {});

private:
    nlohmann::json mergeJson(nlohmann::json base, const nlohmann::json& override);
};
```

**Perfiles predefinidos:**

```json
// config/profiles/quick_inspect.json
{
  "extends": "default_analysis",
  "analysis": {
    "binning": { "axial_bins": 101 },
    "envelope": { "proxy_resample_points": 48, "quantile": 0.95 },
    "focus": { "smooth_curve": false }
  },
  "preview": {
    "trajectories": 1000,
    "samples_per_trajectory": 50
  },
  "storage": {
    "sqlite_threshold_mb": 50
  }
}

// config/profiles/full_analysis.json
{
  "extends": "default_analysis",
  "analysis": {
    "binning": { "axial_bins": 1001 },
    "envelope": { "proxy_resample_points": 256, "angular_sectors": 256 },
    "focus": { "smooth_curve": true },
    "surfaces": { "lens_boundary_points": 256, "lens_radial_layers": 32 }
  },
  "export": {
    "png": { "plot_width": 3800, "plot_height": 2600 }
  }
}

// config/profiles/batch_server.json (headless, optimizado para velocidad)
{
  "extends": "default_analysis",
  "storage": {
    "sqlite_threshold_mb": 0,
    "db_in_memory": false
  },
  "analysis": {
    "binning": { "axial_bins": 501 }
  },
  "ui": {
    "log_max_lines": 0,
    "progress_update_interval_ms": 1000
  }
}
```

---

#### Paso 1.5: ServiceRegistry v2 — DI container mejorado

**Archivos modificados:**
- `src/app/ServiceRegistry.h`
- `src/app/ServiceRegistry.cpp`

**Mejoras:**
- Registrar factories (lazy initialization)
- Scope: Singleton, Transient, Scoped (por dataset)
- Auto-wiring por tipo (sin strings mágicos)
- Validación de dependencias circulares en debug

---

#### Checklist Fase 1

- [ ] `EventBus` compila y pasa tests unitarios (publish/subscribe/unsubscribe).
- [ ] `CommandBus` compila y despacha comandos a handlers registrados.
- [ ] `PluginHost` carga un plugin dummy desde shared library (.so).
- [ ] `ProfileManager` carga 4 perfiles y resuelve merge default→profile→user→cli.
- [ ] `ServiceRegistry` soporta registración lazy y detección de dependencias circulares.
- [ ] Todos los tests existentes (41/41) siguen pasando.
- [ ] Build sin warnings nuevos (`-Wall -Wextra -Wpedantic`).

---

### FASE 2: Storage Revolution — Backend Multi-Formato

**Objetivo:** Reemplazar el storage monolítico con backends intercambiables, optimizar SQLite para 20 GB, agregar caché de consultas frecuentes, y exponer todo vía repositorio.

**Duración:** 16 horas
**Prioridad:** ALTA — el storage es el cuello de botella para datasets grandes

---

#### Paso 2.1: IStorageBackend unificado

**Archivos nuevos:**
- `src/services/storage/IStorageBackend.h`

**Archivos modificados:**
- `src/core/storage/ISampleStorage.h` → rename/migrate
- `src/core/storage/InMemoryStorage.h/.cpp` → `src/services/storage/InMemoryBackend.h/.cpp`
- `src/core/storage/SqliteStorage.h/.cpp` → `src/services/storage/SqliteBackend.h/.cpp`

**Implementación:**

```cpp
// src/services/storage/IStorageBackend.h
class IStorageBackend {
public:
    virtual ~IStorageBackend() = default;

    // Metadata
    virtual std::string backendName() const = 0;
    virtual uint64_t totalSamples() const = 0;
    virtual uint64_t totalTrajectories() const = 0;

    // Escritura batch
    virtual void beginWriteBatch() = 0;
    virtual void writeSample(const TrajectorySample& sample) = 0;
    virtual void endWriteBatch() = 0;

    // Lectura batch
    virtual SampleBatch readBatch(uint64_t offset, uint64_t count) const = 0;
    virtual SampleBatch readAxialRange(double zMin, double zMax) const = 0;
    virtual SampleBatch readTrajectory(TrajectoryId id) const = 0;

    // Estadísticas globales (optimizadas con SQL aggregates)
    virtual struct GlobalStats { double zMin, zMax, edepSum; uint64_t count; }
    globalStats() const = 0;

    // Mantenimiento
    virtual void vacuum() = 0;       // Compactar base de datos
    virtual uint64_t diskUsage() const = 0;
};

// ✨ NUEVO: backend experimental mmap-backed para máxima velocidad
class MmapBackend : public IStorageBackend { /* ... */ };
```

---

#### Paso 2.2: SqliteBackend optimizado para Big Data

**Archivo:** `src/services/storage/SqliteBackend.cpp`

**Optimizaciones:**

```cpp
// 1. PRAGMAs de rendimiento (ya parcialmente implementados)
void SqliteBackend::applyPerformancePragmas() {
    exec("PRAGMA journal_mode=WAL");           // Write-Ahead Logging
    exec("PRAGMA synchronous=NORMAL");         // Seguro con WAL
    exec("PRAGMA cache_size=-65536");          // 64 MB page cache
    exec("PRAGMA temp_store=MEMORY");          // Temp tables en RAM
    exec("PRAGMA mmap_size=268435456");        // 256 MB memory-mapped I/O
    exec("PRAGMA page_size=4096");             // Tamaño de página óptimo
    exec("PRAGMA foreign_keys=OFF");           // Sin FK overhead
    exec("PRAGMA automatic_index=OFF");        // Índices manuales
}

// 2. Inserción con prepared statement cache (reutilizar stmt)
class PreparedStatementCache {
    sqlite3_stmt* get(const std::string& sql);
    void recycle(sqlite3_stmt* stmt);  // sqlite3_reset + sqlite3_clear_bindings
};

// 3. Índices diferidos (crear DESPUÉS de insertar, no antes)
void SqliteBackend::createIndexesAfterImport() {
    exec("CREATE INDEX IF NOT EXISTS idx_axial ON samples(z_m)");
    exec("CREATE INDEX IF NOT EXISTS idx_traj_step ON samples(trajectory_id, step_index)");
    exec("CREATE INDEX IF NOT EXISTS idx_edep ON samples(edep_MeV) WHERE edep_MeV > 0");
    exec("ANALYZE samples");  // Actualizar estadísticas para query planner
}

// 4. Batch insert optimizado
void SqliteBackend::endWriteBatch() {
    exec("BEGIN TRANSACTION");
    for (auto& s : pending_) {
        sqlite3_bind_int64(stmt_, 1, s.trajectoryId.id);
        // ... bind remaining 11 columns
        sqlite3_step(stmt_);
        sqlite3_reset(stmt_);
    }
    exec("COMMIT");
    pending_.clear();
}
```

---

#### Paso 2.3: StorageManager con selección automática

**Archivo nuevo:** `src/services/storage/StorageManager.h/.cpp`

```cpp
class StorageManager {
public:
    // Factory method mejorado
    static std::unique_ptr<IStorageBackend> create(
        uint64_t estimatedFileSize,
        const AnalysisConfig& config);

    // Cache de consultas frecuentes (LRU, thread-safe)
    std::optional<SampleBatch> getCachedBatch(uint64_t offset, uint64_t count);
    void invalidateCache();

    // Streaming: escribe samples desde el importer
    void importFrom(IImporter& importer,
                    const std::string& path,
                    ProgressCallback onProgress);

private:
    struct StorageDecision {
        uint64_t threshold = 100ULL * 1024 * 1024; // 100 MB default
        bool preferDisk = false;  // Forzar SQLite desde config
    };
    StorageDecision decide_;
};
```

---

#### Paso 2.4: ITrajectoryRepository — abstracción de acceso a datos

**Archivo nuevo:** `src/data/repositories/ITrajectoryRepository.h`
**Archivo nuevo:** `src/data/repositories/SqliteTrajectoryRepository.h/.cpp`

```cpp
class ITrajectoryRepository {
public:
    virtual ~ITrajectoryRepository() = default;

    // Consultas
    virtual std::vector<TrajectoryId> getAllTrajectoryIds() const = 0;
    virtual std::optional<TrajectorySummary> getSummary(TrajectoryId id) const = 0;
    virtual SampleBatch getSamples(TrajectoryId id) const = 0;

    // Consultas agregadas (usan SQL, no C++)
    virtual std::vector<FrameStatistics> computeFrameStats(
        const AxisFrame& axis, const BinningConfig& binning) = 0;

    // Mutaciones
    virtual void insertTrajectory(TrajectoryId id, const TrajectoryMetadata& meta) = 0;
    virtual void insertSample(const TrajectorySample& sample) = 0;
};
```

---

#### Checklist Fase 2

- [ ] `IStorageBackend` interfaz definida y documentada.
- [ ] `SqliteBackend` pasa test: insertar 1M samples, leer batch, leer rango axial.
- [ ] `MmapBackend` compila (aunque no sea el default).
- [ ] `StorageManager::create()` selecciona backend correcto por tamaño.
- [ ] `ITrajectoryRepository` implementado con SQLite, pasa tests unitarios.
- [ ] Import de CSV de 1 GB usa <500 MB RAM y completa en <20s.
- [ ] Benchmarks de storage documentados (insert rate, query rate, disk usage).
- [ ] Tests existentes (41/41) siguen pasando.

---

### FASE 3: Analysis Engine — Pipeline Nativo y Paralelo

**Objetivo:** Eliminar la dependencia de bash/Python/QProcess. Implementar un pipeline de análisis 100% nativo C++ con ejecución paralela de engines independientes, progreso real (bytes/ETA), y resultados reproducibles bit a bit.

**Duración:** 20 horas
**Prioridad:** CRÍTICA — el pipeline actual es el principal punto de fallo y lentitud

---

#### Paso 3.1: IAnalysisEngine — interfaz plugin para motores de análisis

**Archivo nuevo:** `src/services/analysis/engines/IAnalysisEngine.h`

```cpp
class IAnalysisEngine {
public:
    virtual ~IAnalysisEngine() = default;

    // Identidad
    virtual std::string name() const = 0;
    virtual std::string version() const = 0;

    // Capacidades
    virtual bool requiresBinnedData() const = 0;     // Necesita binning previo?
    virtual bool requiresFullDataset() const = 0;    // O funciona con batches?
    virtual size_t estimatedMemoryBytes(uint64_t sampleCount) const = 0;

    // Ejecución
    virtual EngineResult execute(
        const IStorageBackend& storage,
        const AnalysisConfig& config,
        const AxisFrame& axis,
        ProgressCallback onProgress) = 0;

    // Validación
    virtual std::vector<std::string> validateConfig(const AnalysisConfig&) const = 0;
};
```

---

#### Paso 3.2: Refactorizar engines existentes a plugins

**Archivos modificados:**

Cada engine existente se adapta a `IAnalysisEngine`:

| Engine actual | Nuevo wrapper | Archivo |
|---------------|---------------|---------|
| `FrameStatisticsEngine` | `FrameStatisticsPlugin` | `src/services/analysis/engines/FrameStatisticsEngine.h/.cpp` |
| `BatchStatisticsEngine` | `BatchStatisticsPlugin` | `src/services/analysis/engines/BatchStatisticsEngine.h/.cpp` |
| `FocusDetector` | `FocusDetectorPlugin` | `src/services/analysis/engines/FocusDetector.h/.cpp` |
| `ConvexHullEnvelopeExtractor` | `ConvexHullEnvelopePlugin` | `src/services/analysis/engines/EnvelopeExtractor.h/.cpp` |
| `FullEnvelopePreviewBuilder` | `FullEnvelopePreviewPlugin` | `src/services/analysis/engines/EnvelopePreviewBuilder.h/.cpp` |
| `SurfaceBuilder` | `SurfaceBuilderPlugin` | `src/services/analysis/engines/SurfaceBuilder.h/.cpp` |
| `LensDiskBuilder` | `LensDiskBuilderPlugin` | `src/services/analysis/engines/LensDiskBuilder.h/.cpp` |
| `AnalysisQualityChecker` | `QualityCheckerPlugin` | `src/services/analysis/engines/QualityChecker.h/.cpp` |
| `StoppingPowerEngine` | `StoppingPowerPlugin` | `src/domain/physics/StoppingPowerEngine.h/.cpp` |

**Patrón de migración para cada engine:**
```cpp
// Antes (acoplado):
class FrameStatisticsEngine {
    void compute(const TrajectoryDataset& ds, ...);  // Recibe dataset completo
};

// Después (plugin):
class FrameStatisticsPlugin : public IAnalysisEngine {
    std::string name() const override { return "FrameStatistics"; }
    std::string version() const override { return "2.0"; }

    EngineResult execute(const IStorageBackend& storage,
                         const AnalysisConfig& config,
                         const AxisFrame& axis,
                         ProgressCallback onProgress) override
    {
        // Lee batches del storage en vez del dataset completo
        uint64_t totalSamples = storage.totalSamples();
        uint64_t batchSize = config.storage.batchSize;

        for (uint64_t offset = 0; offset < totalSamples; offset += batchSize) {
            if (cancelled_.load()) return EngineResult::cancelled();

            auto batch = storage.readBatch(offset, batchSize);
            processBatch(batch, axis);

            float progress = static_cast<float>(offset) / totalSamples;
            onProgress(progress, "Computing frame statistics...");
        }
        return buildResult();
    }
};
```

---

#### Paso 3.3: JobScheduler — ejecución paralela de engines

**Archivo nuevo:** `src/services/analysis/JobScheduler.h/.cpp`

```cpp
class JobScheduler {
public:
    explicit JobScheduler(size_t threadCount = std::thread::hardware_concurrency());

    // Planifica engines independientes en paralelo
    // Detecta automáticamente qué engines pueden correr simultáneamente
    AnalysisResult runAll(
        const std::vector<std::unique_ptr<IAnalysisEngine>>& engines,
        const IStorageBackend& storage,
        const AnalysisConfig& config,
        ProgressCallback onOverallProgress);

private:
    // Dependency graph: engine A → engine B si B necesita output de A
    DirectedGraph<IAnalysisEngine*> buildDependencyGraph(
        const std::vector<std::unique_ptr<IAnalysisEngine>>& engines);

    // Topological sort + parallel execution de niveles independientes
    void executeLevel(const std::vector<IAnalysisEngine*>& level, ...);

    ThreadPool pool_;
    std::atomic<bool> cancelled_{false};
};
```

**Paralelismo automático detectado:**

```
Nivel 1 (parallelizable):
  FrameStatisticsEngine | FocusDetector | EnvelopeExtractor
  (independientes — solo leen storage)

Nivel 2 (depende de Nivel 1):
  FullEnvelopePreviewBuilder (usa resultado de EnvelopeExtractor)
  SurfaceBuilder (usa resultado de FocusDetector)

Nivel 3 (depende de Nivel 2):
  LensDiskBuilder (usa resultado de SurfaceBuilder)
  QualityChecker (usa todos los resultados anteriores)
```

---

#### Paso 3.4: AnalysisOrchestrator v2

**Archivo modificado:** `src/services/analysis/AnalysisOrchestrator.h/.cpp`

```cpp
class AnalysisOrchestrator {
public:
    AnalysisResult run(
        const std::string& csvPath,        // O cualquier formato soportado
        const AnalysisConfig& config,
        ProgressCallback onProgress,
        CompletionCallback onDone);

    // ✨ NUEVO: cancela análisis en progreso
    void cancel();

    // ✨ NUEVO: estima tiempo restante
    struct Estimate {
        std::chrono::seconds estimatedRemaining;
        double bytesPerSecond;
        int percentComplete;
    };
    Estimate getEstimate() const;

private:
    std::unique_ptr<IStorageBackend> storage_;
    JobScheduler scheduler_;
    std::atomic<bool> cancelled_{false};
};
```

---

#### Paso 3.5: Migrar MainWindow a usar nuevo pipeline asíncrono

**Archivo modificado:** `src/ui/qt/MainWindow.cpp`

```cpp
// ANTES (bloqueante, ~100 líneas):
void MainWindow::openDataFileAndRunWithPath(const QString& path) {
    // QProcess → bash → beamlab CLI → esperar → parsear output
    QProcess process;
    process.start("bash", {"-c", cmd});
    process.waitForFinished(-1);  // BLOQUEA UI
    // ...
}

// DESPUÉS (asíncrono, ~20 líneas):
void MainWindow::onOpenFileClicked() {
    QString path = QFileDialog::getOpenFileName(this, "Open Data File",
        QString(), "CSV Files (*.csv);;All Files (*)");

    if (path.isEmpty()) return;

    // Delegar al Presenter
    analysisPresenter_->startAnalysis(path.toStdString());
}

// En AnalysisPresenter:
void AnalysisPresenter::startAnalysis(const std::string& path) {
    showProgressBar("Importing...");

    orchestrator_->run(path, config_,
        // Progress callback (llamado desde thread worker)
        [this](float pct, const std::string& stage) {
            QMetaObject::invokeMethod(this, [=] {
                progressBar_->setValue(static_cast<int>(pct * 100));
                statusLabel_->setText(QString::fromStdString(stage));
            }, Qt::QueuedConnection);
        },
        // Completion callback
        [this](AnalysisResult result) {
            QMetaObject::invokeMethod(this, [=] {
                hideProgressBar();
                if (result.success) {
                    loadResultsIntoUI(result);
                } else {
                    showError(result.error);
                }
            }, Qt::QueuedConnection);
        }
    );
}
```

---

#### Checklist Fase 3

- [ ] `IAnalysisEngine` interfaz definida. 9 engines implementan la interfaz.
- [ ] `JobScheduler` ejecuta engines independientes en paralelo (verificar con timeline de threads).
- [ ] `AnalysisOrchestrator` completa pipeline sin bash/Python/QProcess.
- [ ] Progreso real: barra de progreso muestra % basado en bytes procesados.
- [ ] Cancelación: botón "Cancel" detiene análisis en <2 segundos.
- [ ] UI no se congela durante análisis (verificar con `QElapsedTimer` en main thread).
- [ ] Resultados numéricos idénticos al pipeline anterior (validar con synthetic dataset TC01–TC06).
- [ ] `MainWindow.cpp` <1,000 líneas después de extraer lógica a Presenters.
- [ ] Tests unitarios nuevos para `JobScheduler`, `AnalysisOrchestrator`, cada engine plugin.

---

### FASE 4: Domain Unification — Física y Materiales

**Objetivo:** Unificar los módulos de física y biología bajo un solo namespace `beamlab::domain`. Implementar el registry pattern para materiales y partículas. Eliminar el último vestigio de lógica de negocio en UI.

**Duración:** 14 horas
**Prioridad:** ALTA — step necesario antes de expandir capacidades físicas

---

#### Paso 4.1: MaterialRegistry — reemplazar BioMaterialLibrary monolítica

**Archivo nuevo:** `src/domain/materials/MaterialRegistry.h/.cpp`
**Archivo modificado:** `src/biosim/materials/BioMaterialLibrary.h/.cpp` → deprecar

```cpp
class MaterialRegistry {
public:
    // Registro
    void registerMaterial(std::string name, Material material);
    void registerMaterialsFromJson(const std::string& jsonPath);

    // Búsqueda
    const Material& get(const std::string& name) const;
    std::optional<std::reference_wrapper<const Material>> find(const std::string& name) const;
    std::vector<const Material*> findByCategory(MaterialCategory category) const;
    std::vector<const Material*> findByProperty(
        std::function<bool(const Material&)> predicate) const;

    // Iteración
    size_t count() const;
    std::vector<std::string> names() const;
    auto begin() const { return materials_.begin(); }
    auto end() const { return materials_.end(); }

    // Custom materials (usuario)
    void addCustom(const Material& mat);
    bool removeCustom(const std::string& name);
    std::vector<const Material*> customMaterials() const;

private:
    std::unordered_map<std::string, Material> materials_;
    std::vector<Material> customMaterials_;  // Persistibles a JSON

    // Precargar 55+ materiales ICRU/NIST al inicio
    void loadBuiltinMaterials();
};
```

**Ventajas sobre BioMaterialLibrary:**
- Búsqueda O(1) con `unordered_map` en vez de iteración lineal
- Filtros por categoría/propiedad sin recorrer toda la lista
- Separación clara: built-in (inmutables) vs custom (mutables)
- Serialización directa a JSON para custom materials

---

#### Paso 4.2: ParticleRegistry — mismo patrón para partículas

**Archivo nuevo:** `src/domain/physics/ParticleRegistry.h/.cpp`
**Archivo modificado:** `src/biosim/physics/ParticleLibrary.h/.cpp` → deprecar

Mismo patrón que MaterialRegistry, adaptado a 18 especies de partículas PDG 2022.

---

#### Paso 4.3: SimulationEngine fachada unificada

**Archivo nuevo:** `src/domain/simulation/SimulationEngine.h/.cpp`

```cpp
class SimulationEngine {
public:
    // Constructor recibe registries por inyección
    SimulationEngine(const MaterialRegistry& materials,
                     const ParticleRegistry& particles);

    // API de alto nivel — oculta complejidad de StoppingPower, Straggling, MCS
    struct StepResult {
        double dEdx_MeV_cm;
        double energyLoss_MeV;
        double mcsAngle_rad;
        double mcsDisplacement_cm;
        double stragglingSigma_MeV;
    };

    StepResult computeStep(double kinE_MeV,
                           double stepLength_cm,
                           const std::string& materialName,
                           const std::string& particleName) const;

    // Bragg peak para un material y partícula
    BraggCurve computeBraggCurve(double initialE_MeV,
                                 const std::string& materialName,
                                 const std::string& particleName) const;

    // Validación contra NIST PSTAR
    ValidationReport validateAgainstNist() const;
};
```

---

#### Paso 4.4: Eliminar src/biosim/ → migrar a src/domain/

**Plan de migración:**

| Origen | Destino |
|--------|---------|
| `src/biosim/physics/` | `src/domain/physics/` |
| `src/biosim/materials/` | `src/domain/materials/` |
| `src/biosim/geometry/` | `src/domain/geometry/` |
| `src/biosim/core/BioSimRunner.*` | `src/domain/simulation/BioSimRunner.*` |
| `src/biosim/core/BioSimConfig.h` | `src/domain/simulation/BioSimConfig.h` |
| `src/biosim/core/BioSimResult.h` | `src/domain/simulation/BioSimResult.h` |
| `src/biosim/ui/qt/` | `src/ui/qt/widgets/` (son widgets, no dominio) |
| `src/biosim/core/ScoringPlane*` | `src/services/analysis/engines/ScoringPlane*` |

**CMakeLists.txt cambios:**
- Eliminar `beamlab_biosim` target
- Agregar `beamlab_domain` target (STATIC library)
- `beamlab_ui` linkea `beamlab_domain` (hereda vía `beamlab_core`?)

---

#### Checklist Fase 4

- [ ] `MaterialRegistry` reemplaza `BioMaterialLibrary` con misma API + mejoras.
- [ ] `ParticleRegistry` reemplaza `ParticleLibrary`.
- [ ] `SimulationEngine` fachada funciona con inyección de registries.
- [ ] Todos los tests de física pasan (StoppingPower, Bragg, Straggling).
- [ ] `src/biosim/` directorio eliminado (código migrado a `src/domain/` y `src/ui/qt/widgets/`).
- [ ] `beamlab_biosim` target eliminado de CMake. `beamlab_domain` agregado.
- [ ] `physics_validation.py` confirma dE/dx ±2% vs NIST PSTAR con nuevo código.
- [ ] BioSim Widgets funcionan exactamente igual en la UI.

---

### FASE 5: Plugin Ecosystem — Import/Export/Visualización

**Objetivo:** Convertir importers, exporters y visualizadores en plugins registrables. Cualquier desarrollador puede agregar soporte para un nuevo formato sin tocar el core. El sistema descubre plugins automáticamente.

**Duración:** 16 horas
**Prioridad:** MEDIA — desbloquea la extensibilidad prometida

---

#### Paso 5.1: ImporterRegistry con auto-detección

**Archivo nuevo:** `src/services/import/ImporterRegistry.h/.cpp`

```cpp
class ImporterRegistry {
public:
    // Registro
    void registerImporter(std::unique_ptr<IImporter> importer);

    // Auto-detección: prueba todos los importers contra el archivo
    struct DetectionResult {
        IImporter* bestMatch;
        ImporterCapabilityScore score;  // 0.0–1.0
        std::vector<IImporter*> alternatives;
    };
    DetectionResult detectImporter(const std::string& filePath);

    // Lista todos los importers disponibles
    std::vector<const IImporter*> availableImporters() const;

    // Importa con el mejor importer detectado
    ImportResult import(const std::string& filePath,
                        IStorageBackend& storage,
                        ProgressCallback onProgress);

private:
    std::vector<std::unique_ptr<IImporter>> importers_;
};
```

**Protocolo de detección para cada IImporter:**
```cpp
// Cada importer expone un método probe() que analiza las primeras
// líneas del archivo y devuelve un score de compatibilidad:
class Geant4CsvImporter : public IImporter {
    ImporterCapabilityScore probe(const std::string& filePath) override {
        // Leer primeras 5 líneas
        // Buscar header esperado: "x_cm,y_cm,z_cm,edep_MeV,..."
        // Si coincide → score 0.95
        // Si columnas similares → score 0.5
        // Si no → score 0.0
    }
};
```

---

#### Paso 5.2: ExporterRegistry — mismo patrón

**Archivo nuevo:** `src/services/export/ExporterRegistry.h/.cpp`

```cpp
class ExporterRegistry {
public:
    void registerExporter(std::unique_ptr<IExporter> exporter);

    // Exporta en todos los formatos registrados
    ExportResult exportAll(const IStorageBackend& storage,
                           const AnalysisResult& result,
                           const std::string& outputDir,
                           const std::vector<std::string>& formats = {});  // empty = all

    // Lista formatos disponibles
    std::vector<std::string> availableFormats() const;
};
```

**Nuevo exporter:** `ParquetExporter` para datasets grandes
```cpp
class ParquetExporter : public IExporter {
    std::string name() const override { return "Apache Parquet"; }
    std::string format() const override { return "parquet"; }

    ExportResult export(const IStorageBackend& storage,
                        const AnalysisResult& result,
                        const std::string& outputPath) override;
};
```

---

#### Paso 5.3: Template para plugins externos

**Archivo:** `src/plugins/importers/template/CMakeLists.txt`
**Archivo:** `src/plugins/importers/template/ExampleImporter.h`
**Archivo:** `src/plugins/importers/template/ExampleImporter.cpp`
**Archivo:** `docs/PLUGIN_DEVELOPMENT.md`

```cpp
// Template mínimo para un plugin importer:
#include <beamlab/services/import/IImporter.h>

class ExampleImporter : public beamlab::services::IImporter {
public:
    std::string name() const override { return "ExampleFormat"; }
    std::vector<std::string> supportedExtensions() const override {
        return {".example", ".ex"};
    }

    ImporterCapabilityScore probe(const std::string& path) override {
        // Leer magic bytes o header del archivo
        // Retornar score 0.0–1.0
    }

    void import(const std::string& path,
                IStorageBackend& storage,
                ProgressCallback onProgress) override {
        // Leer archivo, escribir samples al storage
    }
};

// Registrar automáticamente:
extern "C" IImporter* create_importer() {
    return new ExampleImporter();
}
```

---

#### Checklist Fase 5

- [ ] `ImporterRegistry` detecta y carga importers built-in + plugins externos.
- [ ] `ExporterRegistry` exporta en todos los formatos registrados.
- [ ] Template de plugin documentado en `PLUGIN_DEVELOPMENT.md`.
- [ ] Un plugin externo (.so) se carga en runtime y aparece en la UI.
- [ ] Probar con un formato nuevo (ej: HDF5) sin tocar el core.
- [ ] `ParquetExporter` funcional (dependencia opcional vía FetchContent).

---

### FASE 6: UI Renaissance — De God Object a Vista Pura

**Objetivo:** Reducir `MainWindow.cpp` de 3,157 a <400 líneas. Implementar sistema de docking, vistas compuestas, y temas. La UI se convierte en una shell que hostea widgets dockeables sin lógica de negocio.

**Duración:** 18 horas
**Prioridad:** ALTA — el síntoma más visible del problema arquitectónico

---

#### Paso 6.1: MainWindow shell — solo infraestructura de ventana

**Archivo modificado:** `src/ui/qt/MainWindow.h` (de 222 a ~80 líneas)
**Archivo modificado:** `src/ui/qt/MainWindow.cpp` (de 3,157 a <400 líneas)

```cpp
class MainWindow : public QMainWindow {
public:
    explicit MainWindow(ApplicationContext& ctx, QWidget* parent = nullptr);

private:
    // Solo construcción de shell
    void setupMenuBar();
    void setupToolBar();
    void setupStatusBar();
    void setupDockSystem();

    // Delegación a vistas
    void registerViews();  // AnalysisView, BioSimView, ExportView

    // Servicios inyectados (no instanciados aquí)
    ApplicationContext& ctx_;
    std::unique_ptr<DockManager> dockManager_;
    std::unique_ptr<CommandBus> commandBus_;

    // UI elements (mínimo)
    QMenuBar* menuBar_;
    QToolBar* toolBar_;
    QStatusBar* statusBar_;
    QTabWidget* viewTabs_;  // Una tab por cada View
};
```

---

#### Paso 6.2: Sistema de Vistas — extraer todo de MainWindow

**Archivos nuevos:**
- `src/ui/views/AnalysisView.h/.cpp`
- `src/ui/views/BioSimView.h/.cpp`
- `src/ui/views/ExportView.h/.cpp`

```cpp
// Cada View es un widget compuesto autocontenido
class AnalysisView : public QWidget {
public:
    explicit AnalysisView(AnalysisPresenter* presenter);

    // No contiene lógica — solo widgets y delegación
private:
    AnalysisPresenter* presenter_;  // Inyectado

    // Widgets internos (privados)
    QSplitter* splitter_;
    Scene3DWidget* scene3D_;
    StatsDashboardWidget* statsDashboard_;
    RunDashboardWidget* runDashboard_;

    void setupLayout();
    void connectSignals();  // Conecta señales de widgets al presenter
};
```

---

#### Paso 6.3: DockManager — sistema de docking

**Archivo nuevo:** `src/ui/qt/dockable/DockManager.h/.cpp`
**Archivo nuevo:** `src/ui/qt/dockable/IDockableWidget.h`

```cpp
class IDockableWidget {
public:
    virtual ~IDockableWidget() = default;
    virtual QString title() const = 0;
    virtual QString id() const = 0;
    virtual QWidget* widget() = 0;
    virtual Qt::DockWidgetArea preferredArea() const { return Qt::RightDockWidgetArea; }
    virtual bool initiallyVisible() const { return true; }
    virtual QKeySequence shortcut() const { return {}; }
};

class DockManager {
public:
    void registerDockable(std::unique_ptr<IDockableWidget> dockable);
    void restoreLayout(const QByteArray& state);
    QByteArray saveLayout() const;

    // Menú "View" autogenerado desde dockables registrados
    QMenu* createViewMenu();
};
```

**Widgets dockeables registrados:**
- `Scene3DWidget` → "3D Viewport" (Center, siempre visible)
- `StatsDashboardWidget` → "Statistics" (Right)
- `RunDashboardWidget` → "Runs" (Left)
- `BioViewport3D` → "Bio Simulator 3D" (Center, tab)
- `SlabEditor3D` → "Slab Editor" (Right)
- `MaterialEditorDialog` → "Materials" (Right, popup)
- `EnergyScaleBar` → "Energy Scale" (Bottom)
- `TrajectoryInspectorPanel` → "Inspector" (Right)
- `ObjViewerWidget` → "OBJ Preview" (Center, tab)

---

#### Paso 6.4: Sistema de Temas

**Archivo nuevo:** `src/ui/qt/styles/ThemeManager.h/.cpp`

```cpp
class ThemeManager {
public:
    enum class Theme { Dark, Light, HighContrast };

    void applyTheme(Theme theme);
    Theme currentTheme() const;
    std::vector<Theme> availableThemes() const;

private:
    QString loadStyleSheet(Theme theme);
    void applyPalette(Theme theme);
};
```

**Temas predefinidos:**
1. **Dark** (default) — fondo #071C17, cyan/blue/purple accents
2. **Light** — fondo blanco, mismos acentos pero más claros
3. **HighContrast** — para accesibilidad y presentaciones

---

#### Paso 6.5: Presenter Layer — separación Vista/Controlador

**Archivos nuevos:**
- `src/ui/qt/presenters/BioSimPresenter.h/.cpp`
- `src/ui/qt/presenters/ExportPresenter.h/.cpp`

**Archivo modificado:**
- `src/ui/qt/presenters/AnalysisPresenter.h/.cpp`

```cpp
// Los Presenters son puentes entre Views (UI) y Services (lógica)
// No heredan de QObject — usan signals Qt para comunicarse con Views

class AnalysisPresenter : public QObject {
    Q_OBJECT

public:
    AnalysisPresenter(AnalysisOrchestrator* orchestrator,
                      EventBus* eventBus);

public slots:
    void startAnalysis(const QString& filePath);
    void cancelAnalysis();
    void loadProfile(const QString& profileName);
    void exportResults(const QString& format);

signals:
    void progressUpdated(int percent, const QString& stage, int etaSeconds);
    void analysisCompleted(AnalysisResult result);
    void errorOccurred(const QString& message);

private:
    AnalysisOrchestrator* orchestrator_;  // Inyectado, no instanciado
    EventBus* eventBus_;                 // Para eventos globales
};
```

---

#### Checklist Fase 6

- [ ] `MainWindow.cpp` < 400 líneas.
- [ ] `MainWindow.h` < 100 líneas.
- [ ] 3 Views implementadas: `AnalysisView`, `BioSimView`, `ExportView`.
- [ ] `DockManager` permite mover/reordenar/cerrar/reabrir widgets.
- [ ] Menú "View" generado automáticamente de dockables registrados.
- [ ] 3 temas funcionales (Dark, Light, HighContrast), toggleable desde menú.
- [ ] Ningún Presenter incluye `#include <QMainWindow>` o `#include <QApplication>`.
- [ ] Layout de docks se persiste y restaura entre sesiones (QSettings).
- [ ] `beamlabstudio.qss` es la única fuente de estilos (sin `setStyleSheet()` inline).
- [ ] 41 tests existentes siguen pasando.

---

### FASE 7: Performance & GPU — Velocidad sin Compromiso

**Objetivo:** Implementar renderizado 3D acelerado, procesamiento paralelo de batches, y caché inteligente para consultas repetitivas. Todo sin sacrificar precisión numérica.

**Duración:** 14 horas
**Prioridad:** MEDIA — la app ya es funcional, pero estos cambios la hacen volar

---

#### Paso 7.1: Scene3DWidget con double buffering y frustum culling

**Archivo modificado:** `src/ui/qt/widgets/Scene3DWidget.cpp`

```cpp
// 1. Double buffering con QPixmap cache
void Scene3DWidget::paintEvent(QPaintEvent*) {
    if (needsRedraw_ || !cachedPixmap_) {
        // Renderizar a pixmap offscreen
        cachedPixmap_ = std::make_unique<QPixmap>(size());
        cachedPixmap_->fill(backgroundColor_);
        {
            QPainter painter(cachedPixmap_.get());
            painter.setRenderHint(QPainter::Antialiasing);
            renderScene(painter);  // Solo se ejecuta cuando hay cambios
        }
        needsRedraw_ = false;
    }
    // Blit del cache a la pantalla (prácticamente gratis)
    QPainter screenPainter(this);
    screenPainter.drawPixmap(0, 0, *cachedPixmap_);

    // Overlays en tiempo real (ejes, grid, selección)
    drawOverlays(screenPainter);
}

// 2. Frustum culling para datasets grandes
bool Scene3DWidget::isVisible(const AABB& bbox) const {
    // Proyectar 8 esquinas del AABB al viewport
    // Si todas están fuera → skip
    for (const auto& corner : bbox.corners()) {
        QPointF screenPos = worldToScreen(corner);
        if (rect().contains(screenPos.toPoint())) return true;
    }
    return false;  // Totalmente fuera de vista
}

// 3. Level-of-detail para trayectorias
void Scene3DWidget::drawTrajectory(const TrajectorySample* samples, size_t count) {
    double dist = cameraDistance();
    size_t stride = (dist > 100.0) ? 8 : (dist > 10.0) ? 2 : 1;  // LOD
    // Dibujar cada stride-ésimo punto
    for (size_t i = 0; i < count; i += stride) { /* ... */ }
}
```

---

#### Paso 7.2: SQLite query cache para estadísticas frecuentes

**Archivo nuevo:** `src/services/storage/QueryCache.h/.cpp`

```cpp
class QueryCache {
public:
    // LRU cache con time-to-live
    template<typename T>
    std::optional<T> get(const std::string& key);

    template<typename T>
    void put(const std::string& key, const T& value,
             std::chrono::seconds ttl = std::chrono::minutes(5));

    void invalidateByPrefix(const std::string& prefix);

private:
    struct CacheEntry {
        std::string key;
        std::vector<uint8_t> serializedValue;  // MessagePack o JSON
        std::chrono::steady_clock::time_point expiresAt;
    };

    std::list<CacheEntry> lruList_;
    std::unordered_map<std::string, decltype(lruList_)::iterator> index_;
    size_t maxEntries_ = 1000;
};
```

---

#### Paso 7.3: Arena allocator para batches

**Archivo nuevo:** `src/core/math/MemoryArena.h`

```cpp
// Evita malloc/free repetitivo en loops de procesamiento batch
class MemoryArena {
public:
    explicit MemoryArena(size_t capacityBytes = 16 * 1024 * 1024);

    template<typename T>
    T* allocate(size_t count = 1);

    void reset();  // "Libera" toda la memoria sin free() real

    size_t usedBytes() const;
    size_t capacityBytes() const;

private:
    std::vector<uint8_t> buffer_;
    size_t offset_ = 0;
};
```

**Uso en BatchStatisticsEngine:**
```cpp
void BatchStatisticsEngine::processBatch(const SampleBatch& batch) {
    MemoryArena arena(32 * 1024 * 1024);  // 32 MB

    // Todas las allocaciones temporales usan la arena
    double* projectedX = arena.allocate<double>(batch.count());
    double* projectedY = arena.allocate<double>(batch.count());
    Vec3* worldPositions = arena.allocate<Vec3>(batch.count());

    // Procesar...
    // ...

    arena.reset();  // "Liberar" todo — O(1), sin heap fragmentation
}
```

---

#### Paso 7.4: Parallel batch processing con ThreadPool

**Archivo modificado:** `src/services/analysis/JobScheduler.cpp`

```cpp
// Procesar batches en paralelo usando work stealing
std::vector<FrameStatistics> processAllBatches(const IStorageBackend& storage) {
    const uint64_t total = storage.totalSamples();
    const uint64_t batchSize = 100'000;
    const uint64_t numBatches = (total + batchSize - 1) / batchSize;

    ThreadPool pool(std::thread::hardware_concurrency());
    std::vector<std::future<FrameStatistics>> futures;

    for (uint64_t i = 0; i < numBatches; ++i) {
        futures.push_back(pool.enqueue([&, i] {
            uint64_t offset = i * batchSize;
            uint64_t count = std::min(batchSize, total - offset);
            auto batch = storage.readBatch(offset, count);
            return processSingleBatch(batch);
        }));
    }

    // Merge resultados parciales
    FrameStatistics global{};
    for (auto& f : futures) {
        global.merge(f.get());  // Reduce thread-safe
    }
    return global;
}
```

---

#### Checklist Fase 7

- [ ] `Scene3DWidget` renderiza >30 FPS con 500k puntos.
- [ ] Double buffering elimina flickering al hacer zoom/pan.
- [ ] `QueryCache` acelera consultas repetidas (medir hit rate >80% en uso típico).
- [ ] `MemoryArena` reduce allocaciones en 90% durante batch processing.
- [ ] Paralelismo: 4 threads procesan batches 3.5x más rápido que 1 thread.
- [ ] Regresión: resultados numéricos idénticos a single-thread.
- [ ] Benchmarks documentados en `tests/performance/`.

---

### FASE 8: Testing Fortress — Cobertura y Confianza

**Objetivo:** Pasar de <40% a >85% de cobertura en módulos core. Agregar tests de integración, regresión y performance. CI/CD con benchmarks automáticos.

**Duración:** 12 horas
**Prioridad:** MEDIA — sin tests, los refactors futuros son peligrosos

---

#### Paso 8.1: Tests unitarios por módulo

**Nuevos archivos de test:**

| Archivo | Módulo bajo test | Criterios de aceptación |
|---------|-----------------|------------------------|
| `tests/unit/test_MaterialRegistry.cpp` | MaterialRegistry | Búsqueda O(1), filtros, custom add/remove |
| `tests/unit/test_ParticleRegistry.cpp` | ParticleRegistry | Búsqueda por PDG code, por nombre |
| `tests/unit/test_SimulationEngine.cpp` | SimulationEngine | dEdx vs NIST ±2%, Bragg peak shape |
| `tests/unit/test_EventBus.cpp` | EventBus | Publish/subscribe/unsubscribe, order |
| `tests/unit/test_CommandBus.cpp` | CommandBus | Dispatch, error handling |
| `tests/unit/test_ImporterRegistry.cpp` | ImporterRegistry | Detection scoring, fallback |
| `tests/unit/test_DockManager.cpp` | DockManager | Save/restore layout |
| `tests/unit/test_JobScheduler.cpp` | JobScheduler | Parallel execution, cancellation |
| `tests/unit/test_QueryCache.cpp` | QueryCache | LRU eviction, TTL expiration |
| `tests/unit/test_ProfileManager.cpp` | ProfileManager | Merge default→profile→user→cli |
| `tests/unit/test_BioSimRunner.cpp` | BioSimRunner | Correctness, abort |
| `tests/unit/test_ConfigLoader.cpp` | ConfigLoader | JSON parse, validation |

---

#### Paso 8.2: Tests de integración end-to-end

**Nuevos archivos:**

```cpp
// tests/integration/test_import_analysis_export_pipeline.cpp
TEST(IntegrationTest, FullPipeline_SmallCSV) {
    // 1. Crear CSV sintético pequeño (100 trayectorias, 10k muestras)
    auto csvPath = createSyntheticCSV(100, 100);

    // 2. Importar
    auto storage = StorageManager::create(FileUtils::fileSize(csvPath), config);
    auto importer = importerRegistry.detectImporter(csvPath);
    importer->import(csvPath, *storage, nullptr);

    // 3. Analizar
    AnalysisOrchestrator orchestrator;
    auto result = orchestrator.run(storage, config);

    // 4. Verificar resultados
    ASSERT_TRUE(result.success);
    EXPECT_GT(result.frameStats.size(), 0);
    EXPECT_TRUE(result.focusResult.has_value());
    EXPECT_GT(result.envelopeGeometry.vertices.size(), 0);

    // 5. Exportar
    auto tmpDir = std::filesystem::temp_directory_path() / "beamlab_test";
    auto exportResult = exporterRegistry.exportAll(*storage, result, tmpDir);
    EXPECT_TRUE(exportResult.success);
    EXPECT_TRUE(std::filesystem::exists(tmpDir / "energy_step_profile.csv"));
}
```

---

#### Paso 8.3: CI pipeline mejorado

**Archivo modificado:** `.github/workflows/build.yml`

```yaml
jobs:
  build-and-test:
    strategy:
      matrix:
        build-type: [Debug, Release]
        compiler: [gcc-13, clang-18]
    steps:
      - uses: actions/checkout@v4
      - name: Configure
        run: cmake -B build -DCMAKE_BUILD_TYPE=${{ matrix.build-type }}
              -DBEAMLAB_ENABLE_QT_UI=ON -DBEAMLAB_ENABLE_ROOT=ON
      - name: Build
        run: cmake --build build -j$(nproc)
      - name: Unit Tests
        run: ctest --test-dir build --output-on-failure -j$(nproc)
      - name: Integration Tests
        run: ctest --test-dir build -L integration --output-on-failure
      - name: Coverage (Debug only)
        if: matrix.build-type == 'Debug'
        run: |
          cmake -B build -DCMAKE_BUILD_TYPE=Debug -DENABLE_COVERAGE=ON
          cmake --build build
          cmake --build build --target coverage
      - name: Benchmarks
        run: cmake --build build --target benchmarks
      - name: Lint
        run: |
          pip install clang-format==18
          find src/ tests/ -name '*.cpp' -o -name '*.h' | xargs clang-format --dry-run --Werror
```

---

#### Paso 8.4: .clang-format y guía de estilo

**Archivo nuevo:** `.clang-format`

```yaml
BasedOnStyle: Google
IndentWidth: 4
ColumnLimit: 100
AccessModifierOffset: -4
AllowShortFunctionsOnASingleLine: Inline
AlwaysBreakTemplateDeclarations: Yes
BreakBeforeBraces: Custom
BraceWrapping:
  AfterClass: true
  AfterFunction: true
  AfterNamespace: false
PointerAlignment: Left
```

**Archivo nuevo:** `docs/CONTRIBUTING.md`

---

#### Checklist Fase 8

- [ ] 12+ nuevos tests unitarios (total >40 tests).
- [ ] 4 tests de integración end-to-end.
- [ ] Cobertura >85% en `src/domain/`, >70% en `src/services/`, >60% en `src/platform/`.
- [ ] CI incluye build matrix (gcc+clang, Debug+Release), tests, coverage, benchmarks, lint.
- [ ] `.clang-format` configurado. Hook pre-commit instalado.
- [ ] `CONTRIBUTING.md` escrito con guía de estilo, git workflow, build instructions.

---

### FASE 9: Python API — BeamLabStudio como Biblioteca

**Objetivo:** Exponer toda la funcionalidad de análisis vía Python para scripts, Jupyter notebooks, y automatización. Sin dependencia de Qt.

**Duración:** 8 horas
**Prioridad:** BAJA (nice-to-have)

---

#### Paso 9.1: pybind11 bindings

**Archivo nuevo:** `src/scripting/pybind11/beamlab_module.cpp`

```cpp
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>
namespace py = pybind11;

PYBIND11_MODULE(beamlab, m) {
    m.doc() = "BeamLabStudio scientific analysis library";

    // Storage
    py::class_<ISampleStorage>(m, "SampleStorage")
        .def("total_samples", &ISampleStorage::totalSamples)
        .def("read_batch", &ISampleStorage::readBatch);

    // Analysis
    py::class_<AnalysisOrchestrator>(m, "AnalysisOrchestrator")
        .def(py::init<>())
        .def("run", &AnalysisOrchestrator::run,
             py::arg("csv_path"),
             py::arg("config") = AnalysisConfig{},
             py::arg("on_progress") = nullptr,
             py::arg("on_done") = nullptr)
        .def("cancel", &AnalysisOrchestrator::cancel);

    // Physics
    py::class_<SimulationEngine>(m, "SimulationEngine")
        .def(py::init<const MaterialRegistry&, const ParticleRegistry&>())
        .def("compute_step", &SimulationEngine::computeStep)
        .def("compute_bragg_curve", &SimulationEngine::computeBraggCurve);

    // Materials
    py::class_<MaterialRegistry>(m, "MaterialRegistry")
        .def(py::init<>())
        .def("get", &MaterialRegistry::get)
        .def("names", &MaterialRegistry::names);

    // Configuration
    py::class_<ProfileManager>(m, "ProfileManager")
        .def("load_profile", &ProfileManager::loadProfile)
        .def("available_profiles", &ProfileManager::availableProfiles);
}
```

**Ejemplo de uso en Python:**
```python
import beamlab

# Cargar perfil de análisis rápido
pm = beamlab.ProfileManager()
config = pm.load_profile("quick_inspect")

# Ejecutar análisis
orch = beamlab.AnalysisOrchestrator()
result = orch.run(
    "/data/muon_run_042.csv",
    config=config,
    on_progress=lambda pct, stage: print(f"{pct:.0%} {stage}")
)

print(f"Focus position: z={result.focus_result.z_min:.3f} m")
print(f"Envelope vertices: {len(result.envelope_geometry.vertices)}")

# Física
materials = beamlab.MaterialRegistry()
particles = beamlab.ParticleRegistry()
sim = beamlab.SimulationEngine(materials, particles)

step = sim.compute_step(
    kinE_MeV=150.0,
    step_length_cm=0.1,
    material_name="Water, Liquid",
    particle_name="proton"
)
print(f"dE/dx = {step.dEdx_MeV_cm:.2f} MeV/cm")
```

---

#### Checklist Fase 9

- [ ] `beamlab` módulo Python compila y es importable.
- [ ] Ejemplo de Jupyter notebook funcional en `src/scripting/examples/`.
- [ ] Misma precisión numérica que C++ (validado con synthetic dataset).
- [ ] API documentada en `docs/API_REFERENCE.md`.

---

### FASE 10: Polish & Ship — Documentación, Release, Comunidad

**Objetivo:** Preparar el proyecto para contribuciones externas, releases automatizadas, y adopción por la comunidad de física médica.

**Duración:** 6 horas
**Prioridad:** BAJA (final)

---

#### Paso 10.1: Documentación viva

**Archivos:**
- `docs/ARCHITECTURE.md` — diagrama de arquitectura actual (Mermaid), decisiones registradas
- `docs/PLUGIN_DEVELOPMENT.md` — guía paso a paso para crear plugins
- `docs/API_REFERENCE.md` — referencia de API C++ y Python
- `CONTRIBUTING.md` — guía de contribución

#### Paso 10.2: Release automatizada

```yaml
# .github/workflows/release.yml
on:
  push:
    tags: ['v*']

jobs:
  release:
    steps:
      - name: Build all targets
      - name: Run full test suite
      - name: Create AppImage (Linux)
      - name: Create MSI installer (Windows)
      - name: Publish to GitHub Releases
      - name: Generate CHANGELOG.md from conventional commits
```

#### Paso 10.3: Actualizar README

- Remover claims de features no implementadas (VTK, PDF avanzado)
- Agregar badges de coverage, benchmarks, Python API
- Agregar sección "Plugins" con lista de plugins disponibles
- Agregar "Quick Start" para Python API

---

#### Checklist Fase 10

- [ ] `ARCHITECTURE.md` publicado con diagrama Mermaid y ADR (Architecture Decision Records).
- [ ] `PLUGIN_DEVELOPMENT.md` permite a un dev externo crear un plugin en <1 hora.
- [ ] Release automatizada: git tag → GitHub Release con binarios.
- [ ] README actualizado refleja estado real del proyecto.
- [ ] CHANGELOG.md generado automáticamente de conventional commits.

---

## 5. Dashboard de Progreso

```
Fase 0: Safety Stop            [············]   0/5   (6h)
Fase 1: Foundation             [············]   0/5   (18h)
Fase 2: Storage Revolution      [············]   0/4   (16h)
Fase 3: Analysis Engine         [············]   0/5   (20h)
Fase 4: Domain Unification      [············]   0/4   (14h)
Fase 5: Plugin Ecosystem        [············]   0/3   (16h)
Fase 6: UI Renaissance          [············]   0/5   (18h)
Fase 7: Performance & GPU       [············]   0/4   (14h)
Fase 8: Testing Fortress        [············]   0/4   (12h)
Fase 9: Python API              [············]   0/1   (8h)
Fase 10: Polish & Ship          [············]   0/3   (6h)
────────────────────────────────────────────────────
TOTAL:                          [············]   0/43  (148h ≈ ~4 semanas)
```

---

## 6. KPI Tracking (Medir para Saber)

| KPI | Actual (v0.2.0) | Fase 3 | Fase 6 | Fase 7 | Objetivo (v3.0) |
|-----|-----------------|--------|--------|--------|-----------------|
| `MainWindow.cpp` líneas | 3,157 | 1,500 | 400 | 400 | <400 |
| RAM máxima CSV 20 GB | OOM | 2 GB | 2 GB | 800 MB | <1 GB |
| Tiempo import 1 GB | 37s | 25s | 25s | 15s | <15s |
| Cobertura tests core | <40% | 50% | 55% | 70% | >85% |
| Tests totales | 7 | 20 | 22 | 35 | >40 |
| Comunidades <0.10 cohesión | 4 | 3 | 1 | 0 | 0 |
| Nodos débilmente conectados | 237 | 100 | 50 | 20 | <20 |
| Bugs activos | 6 | 3 | 0 | 0 | 0 |
| Tiempo build Release | ~100s | ~110s | ~120s | ~120s | <150s |
| Formatos soportados | 3 (CSV,TSV,ROOT) | 3 | 4 | 5 | Plugin-based |
| UI freeze durante análisis | Sí | No | No | No | No |

---

## Apéndice A: Mapa de Migración de Archivos

| Archivo actual | Destino | Fase |
|----------------|---------|------|
| `src/core/storage/ISampleStorage.h` | `src/services/storage/IStorageBackend.h` | F2 |
| `src/core/storage/InMemoryStorage.*` | `src/services/storage/InMemoryBackend.*` | F2 |
| `src/core/storage/SqliteStorage.*` | `src/services/storage/SqliteBackend.*` | F2 |
| `src/biosim/physics/*` | `src/domain/physics/*` | F4 |
| `src/biosim/materials/*` | `src/domain/materials/*` | F4 |
| `src/biosim/geometry/*` | `src/domain/geometry/*` | F4 |
| `src/biosim/core/BioSimRunner.*` | `src/domain/simulation/BioSimRunner.*` | F4 |
| `src/biosim/ui/qt/*` | `src/ui/qt/widgets/*` | F4 |
| `src/analysis/statistics/*` | `src/services/analysis/engines/*` | F3 |
| `src/analysis/focus/*` | `src/services/analysis/engines/*` | F3 |
| `src/analysis/envelope/*` | `src/services/analysis/engines/*` | F3 |
| `src/analysis/surfaces/*` | `src/services/analysis/engines/*` | F3 |
| `src/analysis/quality/*` | `src/services/analysis/engines/*` | F3 |
| `src/io/importers/*` | `src/services/import/*` | F5 |
| `src/io/exporters/*` | `src/services/export/*` | F5 |
| `src/core/pipeline/*` | `src/services/analysis/*` | F3 |
| `src/app/ServiceRegistry.*` | `src/platform/ServiceRegistry.*` | F1 |
| `src/ui/qt/MainWindow.*` | (reducido a <400 líneas, lógica extraída a Views/Presenters) | F6 |
| (nuevo) | `src/platform/EventBus.*` | F1 |
| (nuevo) | `src/platform/CommandBus.*` | F1 |
| (nuevo) | `src/platform/PluginHost.*` | F1 |

---

## Apéndice B: Registro de Decisiones Arquitectónicas (ADR)

| ID | Decisión | Alternativas | Justificación |
|----|----------|-------------|---------------|
| ADR-001 | EventBus como sistema central de comunicación | Signals/Slots directos, Observer pattern | Desacopla módulos que no deberían conocerse. Qt signals requieren QObject. |
| ADR-002 | Plugin system con shared libraries (.so/.dylib/.dll) | Plugins compilados estáticamente | Permite distribución separada. Usuario final no recompila. |
| ADR-003 | CQRS (CommandBus + QueryBus) para operaciones | Llamadas directas a servicios | Separa lecturas (cacheables) de escrituras. Facilita undo/redo futuro. |
| ADR-004 | SQLite como storage primario (no mmap, no Parquet) | MMF, Apache Arrow, LevelDB | SQLite es battle-tested para billones de filas, tiene B-tree gratis, y es header-only. |
| ADR-005 | C++17 (mantener, no migrar a C++20) | C++20 | C++20 no agrega features críticas para este proyecto y podría romper CI. |
| ADR-006 | Profile system para configuración (extensible) | Hardcoded defaults, environment variables | Permite quick/full/clinical/batch modos sin cambiar código. Override por usuario. |
| ADR-007 | Presenter pattern (MVP) sobre MVC | MVC, MVVM | Qt ya tiene Model/View. Presenter es más ligero y no requiere QAbstractItemModel. |
| ADR-008 | pybind11 para Python API | CFFI, ctypes, SWIG | pybind11 es moderno, header-only, compatible con C++17. Mejor ergonomía. |
| ADR-009 | MemoryArena para batch processing | std::vector con reserve, pool allocator | Elimina fragmentation en loops críticos. O(1) reset. Medible. |
| ADR-010 | GitHub Actions CI con build matrix | Jenkins, GitLab CI | Ya integrado, gratuito para repo público. Matrix gcc/clang Debug/Release. |

---

## Apéndice C: Glosario de Términos del Blueprint

| Término | Definición |
|---------|-----------|
| **EventBus** | Sistema pub/sub que permite a módulos comunicarse sin conocerse. Un módulo publica `ImportCompleted`, otro se suscribe. |
| **CommandBus** | Despachador de comandos CQRS. Cada tipo de comando tiene exactamente un handler. |
| **Plugin** | Biblioteca compartida (.so/.dylib/.dll) que implementa una interfaz conocida y es descubierta en runtime. |
| **Profile** | Archivo JSON con configuración pre-empaquetada para un caso de uso (quick, full, clinical, batch). |
| **View** | Widget compuesto autocontenido que representa una pantalla completa (AnalysisView, BioSimView). |
| **Presenter** | Capa intermedia entre View y Services. Maneja lógica de presentación sin conocer widgets Qt. |
| **Registry** | Colección indexada O(1) que reemplaza listas lineales. Ej: MaterialRegistry, ParticleRegistry. |
| **Arena** | Allocator de memoria que reserva un bloque grande y asigna de él secuencialmente. O(1) reset. |
| **LOD** | Level of Detail — reducir complejidad de renderizado para objetos lejanos. |
| **CQRS** | Command Query Responsibility Segregation — separar operaciones de escritura (comandos) de lectura (queries). |

---

*Documento generado como blueprint arquitectónico para BeamLabStudio v3.0.*
*Debe revisarse al inicio de cada fase. Los hallazgos del grafo de conocimiento deben usarse para verificar que la cohesión mejora después de cada fase.*
