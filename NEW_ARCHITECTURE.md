# NEW_ARCHITECTURE.md — BeamLabStudio v2.0

> **Fecha:** 2026-05-20
> **Autor:** DeepSeek V4 Pro (Lead Architect)
> **Estado:** Propuesta — Pendiente de aprobación

---

## 1. Resumen Ejecutivo

### Problema actual
BeamLabStudio es una app Qt6/C++17 de análisis de trayectorias de partículas (muones) que procesa CSVs exportados de Geant4/ROOT. Actualmente carga todo el dataset en `std::vector<TrajectorySample>` en RAM, lo que causa OOM con archivos > 1 GB (un CSV de 20 GB con 96M filas requiere ~25-30 GB de RAM; la máquina tiene 14 GB). Además, `MainWindow.cpp` tiene 3294 líneas mezclando UI, análisis y exportación, y el pipeline de análisis depende de un script bash lanzado vía QProcess sin feedback de progreso real.

### Visión objetivo
Una aplicación científica modular donde: (1) los samples se almacenan en SQLite para datasets grandes y en memoria para pequeños, con O(1) RAM respecto al tamaño del archivo; (2) el análisis se ejecuta en un pipeline nativo C++ con callbacks de progreso real (bytes/MB procesados, ETA); (3) `MainWindow.cpp` queda en <800 líneas como capa de Vista pura, delegando en Presenters y Managers inyectados; (4) toda la configuración es declarativa vía JSON, sin magic numbers en código.

### Métricas de éxito
| Métrica | Actual | Objetivo |
|---------|--------|----------|
| RAM máxima para CSV 20 GB | OOM (>30 GB) | <2 GB |
| Líneas `MainWindow.cpp` | 3294 | <800 |
| Tiempo de build (Release) | ~100s | <120s (similar) |
| Cobertura de tests unitarios | 5 módulos | >80% en módulos core |
| Parámetros hardcodeados | ~15 | 0 |
| Fuentes de estilo CSS | 1 (.qss) | 1 (.qss) — ya consolidado |

---

## 2. Auditoría del Estado Actual

### 2.1 Diagrama de dependencias actual (simplificado)

```
beamlab_ui (Qt6::Widgets)
├── MainWindow.cpp (3294 líneas) ──── UI + análisis + export TODO junto
│   ├── usa QProcess → scripts/run_beamlab_full.sh → beamlab CLI
│   ├── usa FrameStatisticsEngine, FocusDetector, SurfaceBuilder
│   ├── usa Geant4CsvImporter (vía CLI)
│   └── contiene ExportManager (extraído parcialmente)
├── Scene3DWidget ─── renderer QPainter puro 3D
├── StatsDashboardWidget ─── gráficos + tablas
├── BioSimWidget ─── simulador biológico 3D
│   └── usa BioSimRunner → StoppingPowerEngine (Sternheimer)
├── beamlab_biosim (STATIC lib)
│   ├── StoppingPowerEngine (Bethe-Bloch + Sternheimer + MCS)
│   ├── BioMaterialLibrary (55+ materiales ICRU)
│   ├── EnergyStraggling (Gaussiano, Vavilov, Landau)
│   └── BioSimRunner (orquestador de simulación)
└── beamlab_core (STATIC lib)
    ├── FrameStatisticsEngine, FocusDetector, SurfaceBuilder
    ├── Geant4CsvImporter (streaming input)
    ├── EnergyProfileExporter, QualityReportExporter
    └── ScoringPlaneDetector

beamlab (CLI, sin Qt)
├── ApplicationBootstrap.cpp
└── usa beamlab_core
```

### 2.2 Módulo vs Responsabilidad vs Deuda Técnica

| Módulo | Responsabilidad | Líneas | Deuda Técnica |
|--------|----------------|--------|---------------|
| `MainWindow.cpp` | UI + análisis + export + manifiestos | 3294 | Mezcla 4 responsabilidades. Difícil testear. |
| `Geant4CsvImporter` | Parseo CSV Geant4 15-columnas | 549 | Streaming input ✓, acumula TODO en RAM ✗ |
| `ApplicationBootstrap` | Orquestador CLI | 808 | Lógica de pipeline duplicada con UI |
| `StoppingPowerEngine` | Física Bethe-Bloch + Sternheimer | 161 | Correcto. Modelo más avanzado disponible. |
| `FrameStatisticsEngine` | Estadísticas por frame axial | 386 | `projectSamples()` llamada 2 veces (ya corregido) |
| `SurfaceBuilder` | Generación de mallas OBJ | 104 | Orientación de caras corregida |
| `run_beamlab_full.sh` | Script bash de análisis | 33 | Depende de shell + Python. Sin progreso. |
| `defaultAnalysisArguments()` | Parámetros hardcodeados | 15 valores | Magic numbers: `--window 5`, `--caustic-points 256`, etc. |

### 2.3 Decisiones arquitectónicas pasadas problemáticas

1. **Todo en memoria**: `TrajectoryDataset` contiene `std::vector<Trajectory>` de `std::vector<TrajectorySample>`. Esto fue correcto para prototipos con CSVs de <10 MB, pero no escala a 20 GB.

2. **Pipeline externo**: El análisis se lanza como proceso hijo (`QProcess` → `bash script` → `beamlab CLI`). Esto impide feedback de progreso granular (solo se ve stdout), complica el manejo de errores, y requiere shell + Python en PATH.

3. **MainWindow como God Object**: La ventana principal instancia directamente engines de análisis, exportadores, y el pipeline. No hay separación Vista/Modelo.

4. **Parámetros en código**: `defaultAnalysisArguments()` tiene 15 valores mágicos. Cambiar la resolución de bins requiere recompilar.

---

## 3. Principios de Diseño

| # | Principio | Justificación |
|---|-----------|---------------|
| 1 | **Storage-Compute Separation** | El almacenamiento de samples es abstraído detrás de `ISampleStorage`. Los engines de análisis leen batches, no vectores completos. Permite cambiar backend (memoria → SQLite → futuro: Parquet) sin tocar engines. |
| 2 | **Zero Hardcode** | Todo parámetro configurable vive en `config/default_analysis.json`. La UI y el CLI lo cargan al inicio. Override por CLI o por archivo de usuario. |
| 3 | **View/Presenter Separation** | `MainWindow` es solo Vista. `AnalysisPresenter` y `ExportPresenter` son la capa Controlador. Los Managers (Modelo) no conocen Qt. |
| 4 | **Streaming-First** | Toda operación sobre datasets grandes debe ser O(1) en RAM. Los importers escriben a storage en batches. Los engines leen en batches. |
| 5 | **Compile-Time Safety** | `[[nodiscard]]` en todas las funciones que retornan valor. `const` correctness. `std::optional` en vez de punteros nulos. `tryParseFiniteDouble` en vez de `std::stod` crudo. |
| 6 | **Testable by Construction** | Cada módulo core tiene dependencias inyectadas (storage, config). Testable con mocks sin levantar la UI. |
| 7 | **Progressive Enhancement** | El sistema funciona con in-memory storage para CSVs pequeños. Solo activa SQLite cuando el archivo supera 100 MB. El usuario no elige el backend. |

---

## 4. Arquitectura Objetivo

### 4.1 Estructura de Directorios

```
src/
├── core/                          # Fundaciones: matemática, errores, guards
│   ├── math/                      # Vec3, Vec2, dot(), subtract() [inline]
│   ├── foundation/                # Error, Warning, NumericGuards, StatusCode
│   └── units/                     # Constantes físicas, conversiones
├── config/                        # Sistema de configuración declarativa
│   ├── AnalysisConfig.h           # Struct parseado de JSON
│   └── ConfigLoader.h/cpp         # Lee default + user + CLI overrides
├── storage/                       # Abstracción de almacenamiento (Big Data)
│   ├── ISampleStorage.h           # Interfaz pura
│   ├── InMemoryStorage.h/cpp      # Backend para CSVs < 100 MB
│   ├── SqliteStorage.h/cpp        # Backend SQLite para CSVs grandes
│   └── SampleBatch.h              # Slice de samples para batch processing
├── physics/                       # SIMULACIÓN UNIFICADA (ex-biosim)
│   ├── engine/                    # StoppingPowerEngine, BraggPeakCalculator
│   ├── straggling/                # EnergyStraggling (Gauss, Vavilov, Landau)
│   ├── materials/                 # BioMaterial, BioMaterialLibrary
│   ├── particles/                 # ParticleSpecies, ParticleLibrary
│   └── mcs/                       # Highland MCS (ya integrado)
├── data/                          # Modelos de datos (sin cambios)
│   ├── model/                     # TrajectoryDataset, Trajectory, Sample, etc.
│   └── ids/                       # SampleId, TrajectoryId
├── io/                            # Entrada/Salida
│   ├── import/                    # IImporter, Geant4CsvImporter, BatchImporter
│   ├── export/                    # IExporter, ExportManager, EnergyProfileExporter
│   ├── manifest/                  # ManifestReader, ManifestWriter
│   └── parsing/                   # DelimiterDetector, HeaderRecognizer
├── analysis/                      # Engines de análisis científico
│   ├── statistics/                # FrameStatisticsEngine, BatchFrameStatisticsEngine
│   ├── focus/                     # FocusDetector, FocusCurveBuilder
│   ├── envelope/                  # FullEnvelopePreviewBuilder, AngularEnvelopeExtractor
│   ├── surfaces/                  # SurfaceBuilder, LensDiskBuilder
│   └── orchestrator/              # AnalysisOrchestrator (coordina todos los engines)
├── pipeline/                      # Pipeline de análisis nativo (reemplaza bash)
│   ├── AnalysisPipeline.h/cpp     # Orquestador sin Qt
│   ├── ProgressTracker.h/cpp      # Callbacks de progreso (bytes, ETA)
│   └── PipelineTypes.h            # ProgressCallback, CompletionCallback
├── ui/                            # Interfaz de usuario
│   ├── qt/
│   │   ├── MainWindow.h/cpp       # <800 líneas, solo Vista
│   │   ├── presenters/            # AnalysisPresenter, ExportPresenter (Controlador)
│   │   ├── widgets/               # Scene3DWidget, ObjViewerWidget, Dashboard, BioSimWidget
│   │   └── styles/                # beamlabstudio.qss (única fuente)
│   └── cli/                       # main.cpp (CLI, delega en AnalysisPipeline)
tests/
├── unit/                          # GoogleTest por módulo
├── integration/                   # End-to-end pipeline tests
└── data/                          # Fixtures pequeños (<1 MB cada uno)
config/
└── default_analysis.json          # Todos los parámetros por defecto
```

### 4.2 Diagrama de Dependencias (ASCII)

```
┌─────────────────────────────────────────────────┐
│                    UI Layer                       │
│  MainWindow ──► AnalysisPresenter                │
│     │               │                            │
│     │               ▼                            │
│     │         AnalysisPipeline                   │
│     │               │                            │
│     ▼               ▼                            │
│  ExportPresenter  ProgressTracker                │
└──────────┬──────────────┬────────────────────────┘
           │              │
           ▼              ▼
┌──────────────────────────────────────────────────┐
│              Analysis Layer                       │
│  AnalysisOrchestrator                             │
│  ├── FrameStatisticsEngine (batch)                │
│  ├── FocusDetector                                │
│  ├── SurfaceBuilder                               │
│  └── FullEnvelopePreviewBuilder                   │
│          │                                        │
│          ▼                                        │
│     ISampleStorage ◄── InMemoryStorage             │
│                    ◄── SqliteStorage               │
└──────────────────────┬───────────────────────────┘
                       │
                       ▼
┌──────────────────────────────────────────────────┐
│               IO Layer                             │
│  Geant4CsvImporter ──► ISampleStorage              │
│  ExportManager ──► PNG/MP4/PDF/OBJ/CSV            │
│  ManifestReader/Writer                             │
└──────────────────────┬───────────────────────────┘
                       │
                       ▼
┌──────────────────────────────────────────────────┐
│             Physics Layer                          │
│  StoppingPowerEngine (Bethe-Bloch + Sternheimer)   │
│  EnergyStraggling (Gauss/Vavilov/Landau)           │
│  BraggPeakCalculator                               │
│  BioMaterialLibrary                                │
└──────────────────────┬───────────────────────────┘
                       │
                       ▼
┌──────────────────────────────────────────────────┐
│              Core Layer                            │
│  Vec3, Vec2, dot(), subtract()                    │
│  Error, Warning, NumericGuards                    │
│  AnalysisConfig, ConfigLoader                     │
└──────────────────────────────────────────────────┘
```

### 4.3 Capas y Responsabilidades

| Capa | Claves/Namespaces | Responsabilidad Única |
|------|-------------------|----------------------|
| **UI** | `MainWindow`, `AnalysisPresenter`, `ExportPresenter` | Mostrar widgets, recibir input del usuario, delegar en Presenters |
| **Pipeline** | `AnalysisPipeline`, `ProgressTracker` | Orquestar flujo de análisis, emitir progreso |
| **Analysis** | `AnalysisOrchestrator`, `FrameStatisticsEngine`, `FocusDetector` | Computar estadísticas, foco, superficies sobre batches |
| **IO** | `Geant4CsvImporter`, `ExportManager`, `ManifestReader` | Leer/escribir archivos, parsear formatos |
| **Physics** | `StoppingPowerEngine`, `EnergyStraggling`, `BioMaterialLibrary` | Calcular stopping power, straggling, dosis |
| **Storage** | `ISampleStorage`, `InMemoryStorage`, `SqliteStorage` | Almacenar y recuperar samples por batch |
| **Core** | `Vec3`, `Error`, `AnalysisConfig` | Tipos fundamentales, configuración, guards numéricos |

### 4.4 Patrones de Diseño Usados

| Patrón | Dónde | Por qué |
|--------|-------|--------|
| **Strategy** | `ISampleStorage` → `InMemoryStorage` / `SqliteStorage` | El backend de almacenamiento se selecciona según tamaño del archivo, sin cambiar los engines |
| **Presenter (MVP)** | `AnalysisPresenter` entre `MainWindow` y `AnalysisPipeline` | La Vista no conoce el Modelo. El Presenter recibe callbacks y actualiza la UI vía señales Qt |
| **Factory Method** | `ISampleStorage::create(fileSize)` | La factoría decide qué backend instanciar basado en el tamaño del archivo |
| **Observer** | `ProgressTracker` → callbacks a `AnalysisPresenter` | El pipeline emite progreso sin conocer Qt. El Presenter traduce a `QProgressBar::setValue()` |
| **Facade** | `AnalysisOrchestrator` | Unifica la interfaz de múltiples engines (estadísticas, foco, superficies) para el pipeline |
| **Dependency Injection** | Constructores de engines reciben `ISampleStorage&` | Testable: pasar `InMemoryStorage` con datos sintéticos en tests unitarios |

---

## 5. Subsistemas Detallados

### 5.1 Almacenamiento de Samples (Big Data)

#### Estrategia elegida: SQLite vía amalgamation

**¿Por qué SQLite y no otras opciones?**

| Opción | Ventaja | Desventaja | Veredicto |
|--------|---------|------------|-----------|
| **SQLite** | B-tree indexing gratis, ACID, single-file, header-only, battle-tested para billones de filas | INSERT más lento que binary crudo (~50k filas/s batch) | **Elegido** — Mejor balance simplicidad/rendimiento |
| **Memory-Mapped Files** | Acceso O(1) a cualquier offset, cero overhead de serialización | Sin índice built-in = range queries lineales. Requiere formato binario fijo (no portable). Complejo para tipos variables (strings). | Rechazado |
| **Binary chunks** (`.bin` + `.idx`) | Máxima velocidad, formato custom | Hay que diseñar, implementar y debuggear el formato. Cada cambio de schema rompe compatibilidad. | Rechazado |
| **Apache Arrow/Parquet** | Formato estándar, compresión | Dependencia pesada (Arrow >100MB). No justificado para este proyecto. | Rechazado |
| **LevelDB/LMDB** | Key-value rápido | Solo key-value, sin SQL para queries analíticas. | Rechazado |

#### Esquema SQLite

```sql
CREATE TABLE IF NOT EXISTS samples (
    sample_id    INTEGER PRIMARY KEY AUTOINCREMENT,
    trajectory_id INTEGER NOT NULL,
    step_index   INTEGER NOT NULL,
    x_m          REAL NOT NULL,
    y_m          REAL NOT NULL,
    z_m          REAL NOT NULL,
    edep_MeV     REAL NOT NULL DEFAULT 0.0,
    kinE_MeV     REAL NOT NULL DEFAULT 0.0,
    momx_MeV     REAL NOT NULL DEFAULT 0.0,
    momy_MeV     REAL NOT NULL DEFAULT 0.0,
    momz_MeV     REAL NOT NULL DEFAULT 0.0,
    time_s       REAL NOT NULL DEFAULT 0.0,
    dose_Gy      REAL NOT NULL DEFAULT 0.0
);

CREATE INDEX IF NOT EXISTS idx_traj_step ON samples(trajectory_id, step_index);
CREATE INDEX IF NOT EXISTS idx_axial ON samples(z_m);  -- para binning axial
```

#### Interfaz `ISampleStorage`

```cpp
class ISampleStorage {
public:
    virtual ~ISampleStorage() = default;

    // Escritura (desde el importer)
    virtual void beginTrajectory(uint64_t trajectoryId) = 0;
    virtual void addSample(const TrajectorySample& sample) = 0;
    virtual void endTrajectory() = 0;
    virtual void flush() = 0;  // commit batch a disco

    // Lectura (desde los engines de análisis)
    virtual uint64_t totalSampleCount() const = 0;
    virtual uint64_t trajectoryCount() const = 0;

    // Retorna un slice de samples. El engine itera sobre él.
    virtual SampleBatch getBatch(uint64_t offset, uint64_t count) const = 0;
    virtual SampleBatch getAxialRange(double zMin, double zMax) const = 0;

    // Factory
    static std::unique_ptr<ISampleStorage> create(uint64_t estimatedFileSize);
};
```

#### Estrategia de Batching y Progreso

```
1. Importer lee CSV línea por línea (std::ifstream + std::getline).
2. Cada 1000 líneas, flush() inserta batch en SQLite.
3. ProgressTracker emite "bytes leídos / total bytes" cada 100 ms.
4. UI recibe el callback y actualiza QProgressBar.
5. Al terminar la importación, los engines de análisis leen batches
   de 100k samples vía getBatch(offset, count).
```

### 5.2 Pipeline de Análisis

#### `AnalysisPipeline` (orquestador sin Qt)

```cpp
class AnalysisPipeline {
public:
    using ProgressCb = std::function<void(int64_t bytesRead, int64_t totalBytes)>;
    using DoneCb = std::function<void(bool success, std::string error)>;

    void run(const std::string& csvPath,
             const AnalysisConfig& config,
             ProgressCb onProgress,
             DoneCb onDone);

private:
    std::unique_ptr<ISampleStorage> storage_;
    AnalysisOrchestrator orchestrator_;
    ProgressTracker tracker_;
};
```

#### Flujo de datos completo

```
CSV file (20 GB)
    │
    ▼
Geant4CsvImporter::import()
    │ std::ifstream + std::getline (streaming)
    │ cada 1000 líneas → storage_->addSample()
    ▼
ISampleStorage (SQLite or InMemory)
    │
    ▼
AnalysisOrchestrator
    ├── BatchFrameStatisticsEngine  ← getBatch(offset, 100k)
    ├── FocusDetector               ← getAxialRange()
    ├── SurfaceBuilder              ← boundary points
    └── FullEnvelopePreviewBuilder  ← getBatch()
    │
    ▼
ExportManager
    ├── exportCsv (energy_step_profile.csv)
    ├── exportObj (trajectories_preview.obj)
    ├── exportPng (plots)
    ├── exportMp4 (video)
    └── exportPdf (statistics)
```

### 5.3 Física y Simulación

#### Unificación completada

`src/simulation/` ya fue eliminado (commit `e014f41`). Solo existe `src/biosim/`. Se renombrará a `src/physics/` para claridad.

#### Modelos disponibles

| Modelo | Engine | Precisión | Cuándo usarlo |
|--------|--------|-----------|---------------|
| Bethe-Bloch con Sternheimer | `StoppingPowerEngine` | ±2% vs NIST PSTAR | Siempre (default) |
| MCS Highland | `StoppingPowerEngine::mcsAngle_rad()` | ±5% para 0.001 < t/X0 < 100 | Cálculo de dispersión lateral |
| Straggling Gaussiano | `EnergyStraggling::Mode::Gaussian` | ±15% en cola | Rápido, para visualización |
| Straggling Vavilov | `EnergyStraggling::Mode::Vavilov` | ±3% | Para dosimetría clínica |
| Bragg peak (Bortfeld) | `BraggPeakCalculator` | ±2% en profundidad | Perfil de dosis en profundidad |

#### Fachada `SimulationEngine`

```cpp
class SimulationEngine {
public:
    double dEdx(double kinE_MeV, const BioMaterial& mat,
                const ParticleSpecies& p) const;
    double energyLoss(double kinE_MeV, double dx_cm,
                      const BioMaterial& mat, const ParticleSpecies& p) const;
    double dose_Gy(double edep_MeV, const BioMaterial& mat,
                   double dx_cm, double r_cm) const;
    // ... delega en StoppingPowerEngine
};
```

### 5.4 Importación y Exportación

#### Interfaces

```cpp
class IImporter {
public:
    virtual std::string name() const = 0;
    virtual std::vector<std::string> supportedExtensions() const = 0;
    virtual ImporterCapabilityScore probe(const ProbeResult&) const = 0;
    virtual void import(const std::string& path, ISampleStorage& storage,
                        ProgressCb onProgress) const = 0;
    // Ya no retorna TrajectoryDataset — escribe directo a ISampleStorage
};

class IExporter {
public:
    virtual std::string name() const = 0;
    virtual ExportResult export(const ISampleStorage& storage,
                                const std::string& path) const = 0;
};
```

#### Factory de Importers

```cpp
// En ImportManager:
auto importer = ImportManager::detectImporter(filePath, probeResult);
importer->import(filePath, *storage, onProgress);
```

El `detectImporter` usa `IImporter::probe()` para puntuar cada importer contra el archivo y elige el de mayor score.

### 5.5 UI y Visualización

#### MainWindow reducido a <800 líneas

```cpp
class MainWindow : public QMainWindow {
    // ~200 líneas: setup de UI (menús, toolbars, docks)
    // ~200 líneas: slots que delegan en Presenters
    // ~200 líneas: handlers de drag-drop, file dialogs
    // ~200 líneas: conexiones signal/slot
    // Total: ~800 líneas

private:
    AnalysisPresenter* analysisPresenter_;  // Controlador de análisis
    ExportPresenter*   exportPresenter_;    // Controlador de exportación
    // NO instancia engines, storage, ni pipeline directamente
};
```

#### Sistema de configuración declarativa

`config/default_analysis.json` — ver sección 7.

#### Progreso real con bytes/ETA

```cpp
// En AnalysisPresenter:
void onProgress(int64_t bytesRead, int64_t totalBytes) {
    int pct = static_cast<int>(100.0 * bytesRead / totalBytes);
    progressBar_->setValue(pct);
    double elapsed = timer_.elapsed() / 1000.0;
    double eta = (elapsed / bytesRead) * (totalBytes - bytesRead);
    statusLabel_->setText(QString("Processing... %1% (ETA: %2)")
        .arg(pct).arg(formatDuration(eta)));
}
```

---

## 6. Plan de Migración (Fases)

| Fase | Nombre | Archivos afectados | Riesgo | Tiempo | Commit |
|------|--------|-------------------|--------|--------|--------|
| **A** | Infraestructura: storage + config | `src/storage/*`, `src/config/*`, `config/default_analysis.json`, `CMakeLists.txt` | Bajo | 4h | `arch: add storage abstraction and declarative config` |
| **B** | Batch Engines | `src/analysis/statistics/*`, `src/analysis/orchestrator/*` | Medio | 4h | `arch: add batch analysis engines` |
| **C** | Native Pipeline + Presenter | `src/pipeline/*`, `src/ui/qt/presenters/*`, `MainWindow.cpp` | Alto | 4h | `arch: replace bash pipeline with native AnalysisPipeline` |
| **D** | Polish + Progress | `MainWindow.cpp`, `beamlabstudio.qss` | Bajo | 2h | `arch: add progress bar with ETA to UI` |

### Checklist por Fase

**Fase A:**
- [ ] `cmake --build build` exitoso
- [ ] `config/default_analysis.json` válido (parsea sin error)
- [ ] `InMemoryStorage` pasa test: insert 1000 samples, leer de vuelta
- [ ] `SqliteStorage` pasa test: insert 100k samples, query por rango axial
- [ ] Tests unitarios existentes siguen pasando (20/20)

**Fase B:**
- [ ] `BatchFrameStatisticsEngine` produce mismos resultados que el actual
- [ ] `AnalysisOrchestrator` coordina 3 engines sin crash
- [ ] Test: CSV de 1000 líneas → mismos stats que implementación actual
- [ ] Rollback plan: los engines originales no se borran, solo se añaden los batch

**Fase C:**
- [ ] `AnalysisPipeline::run()` completa sin crash con CSV pequeño
- [ ] ProgressCallback se dispara al menos 10 veces durante import
- [ ] UI responde durante el análisis (no se congela)
- [ ] Rollback plan: `openDataFileAndRunWithPath()` original se preserva como fallback

**Fase D:**
- [ ] `QProgressBar` muestra porcentaje real
- [ ] ETA se actualiza cada segundo
- [ ] CSV sintético de 1 GB procesa con RAM < 500 MB
- [ ] `MainWindow.cpp` < 800 líneas

---

## 7. Configuración y Zero Hardcode

### 7.1 `config/default_analysis.json`

```json
{
  "schema_version": "1.0",
  "storage": {
    "sqlite_threshold_mb": 100,
    "batch_size": 100000,
    "db_in_memory": false
  },
  "import": {
    "max_preview_lines": 12,
    "strict_header": true,
    "expected_columns": [
      "x_cm", "y_cm", "z_cm", "edep_MeV", "kinE_MeV",
      "momx_MeV", "momy_MeV", "momz_MeV", "time_ns",
      "trackID", "parentID", "eventID", "pdg",
      "particleName", "source_file"
    ]
  },
  "analysis": {
    "axis_mode": "auto",
    "reference_mode": "auto",
    "binning": {
      "default_mode": "uniform",
      "axial_bins": 501,
      "half_window_frames": 5
    },
    "focus": {
      "metric": "transverse_rms_radius",
      "smooth_curve": false
    },
    "envelope": {
      "proxy_resample_points": 96,
      "proxy_preview_scale": 1.0,
      "angular_sectors": 128,
      "quantile": 0.90
    },
    "surfaces": {
      "lens_boundary_points": 128,
      "lens_radial_layers": 16,
      "lens_center_thickness_m": 0.0035,
      "lens_edge_thickness_m": 0.0006
    }
  },
  "preview": {
    "trajectories": 10000,
    "samples_per_trajectory": 200,
    "slice_points": 10000
  },
  "export": {
    "mp4": {
      "frame_count": 120,
      "framerate": 30
    },
    "png": {
      "plot_width": 1900,
      "plot_height": 1300
    }
  },
  "ui": {
    "log_max_lines": 10000,
    "progress_update_interval_ms": 100
  }
}
```

### 7.2 Sistema de Overrides

```
Jerarquía de configuración (último gana):
1. config/default_analysis.json     (incluido en recursos Qt)
2. ~/.config/BeamLabStudio/user_config.json  (persiste cambios de UI)
3. --config /path/to/custom.json    (CLI override)

Implementación:
  ConfigLoader::load(args.configOverridePath)
    → lee default
    → mergea user (si existe)
    → mergea CLI override (si se pasó)
    → retorna AnalysisConfig
```

---

## 8. Testing Strategy

| Tipo | Framework | Cobertura objetivo | Ejemplos |
|------|-----------|-------------------|----------|
| **Unit** | GoogleTest + ctest | >80% en módulos core | `FrameStatisticsEngine`, `FocusDetector`, `SurfaceBuilder`, `StoppingPowerEngine`, `Geant4CsvImporter` |
| **Integration** | GoogleTest | 5 casos end-to-end | Import CSV → compute stats → verify values |
| **Physics validation** | Python + NumPy | NIST PSTAR ±2% | `tests/physics_validation.py` — compara dEdx contra tabla NIST |
| **Performance** | Custom (QElapsedTimer) | RAM < 2 GB para CSV 20 GB | Medir RSS del proceso durante import |
| **Regression** | ctest | 20 tests existentes mantienen PASS | `FrameStatisticsEngineTest.*`, `FocusDetectorTest.*`, etc. |

### Nuevos tests a agregar

```
tests/unit/
├── test_InMemoryStorage.cpp      # insert/get/batch
├── test_SqliteStorage.cpp        # insert/get/batch/range
├── test_AnalysisPipeline.cpp     # mock storage, verificar callbacks
├── test_ConfigLoader.cpp         # default/user/override merge
└── test_BatchStatisticsEngine.cpp # mismos resultados que non-batch
```

---

## 9. Decisiones y Trade-offs

| Decisión | Alternativa rechazada | Por qué ganó |
|----------|----------------------|--------------|
| **SQLite** para Big Data | Memory-mapped files | MMF requiere formato binario fijo sin índices. SQLite da B-tree gratis para range queries (clave para binning axial). |
| **C++17** (mantener) | C++20 | El proyecto ya compila en C++17. Migrar a C++20 no da beneficios inmediatos y podría romper compatibilidad con compiladores de CI. |
| **QPainter 3D** (mantener) | VTK | VTK es una dependencia pesada (~200 MB). QPainter es suficiente para visualización de trayectorias y mallas simples. |
| **nlohmann/json** header-only | Qt QJsonDocument | nlohmann/json es más ergonómico para C++ y no requiere Qt (usable desde CLI sin GUI). |
| **SQLite amalgamation** | SQLite vía sistema | La amalgamation (2 archivos) evita dependencia de `libsqlite3-dev` en el sistema. |
| **Batch de 100k samples** | 10k o 1M | 100k es ~12 MB de RAM por batch. Balance entre overhead de I/O y memoria. Calibrable vía config. |
| **Presenter pattern** | MVC clásico | Qt ya tiene Model/View para widgets. Presenter es más ligero y no requiere QAbstractItemModel para todo. |

---

## 10. Riesgos y Mitigaciones

| Riesgo | Prob | Impacto | Mitigación |
|--------|------|--------|------------|
| SQLite más lento que in-memory para CSVs <100 MB | Media | Bajo | Auto-detección: usar InMemoryStorage si archivo <100 MB |
| Batch processing cambia resultados numéricos | Baja | Alto | Validar contra output actual con CSV de 1000 líneas. Si hay diferencias, documentarlas. |
| Pipeline refactor rompe exportaciones existentes | Media | Alto | No eliminar `ExportManager` actual. Añadir `AnalysisPipeline` como nueva ruta, mantener bash como fallback durante transición. |
| Migración rompe UI (ya no abre CSVs) | Baja | Crítico | Preservar `openDataFileAndRunWithPath()` original. Activar nuevo pipeline vía feature flag en config. |
| SQLite amalgamation conflictúa con Qt SQL | Baja | Bajo | Usar símbolos con prefijo o compilar amalgamation en namespace separado. |
| Dependencia circular storage ↔ physics | Baja | Medio | `ISampleStorage` no incluye headers de physics. Los engines reciben el storage por referencia. |

---

## 11. Checklist de Aceptación

- [ ] `cmake --build build` compila en Debug y Release sin errores
- [ ] `ctest` pasa 100% (20 tests existentes + nuevos)
- [ ] CSV pequeño (<1 MB) importa y analiza correctamente (regression)
- [ ] CSV de 1 GB (sintético) procesa sin OOM, RAM <500 MB
- [ ] CSV de 20 GB (real) procesa sin OOM, RAM <2 GB
- [ ] UI abre sin crash tras build limpio
- [ ] Export PNG/MP4/PDF funcionan con datos del nuevo pipeline
- [ ] BioSim produce resultados equivalentes al pipeline anterior (±0.1%)
- [ ] `MainWindow.cpp` < 800 líneas
- [ ] `config/default_analysis.json` contiene TODOS los parámetros antes hardcodeados
- [ ] Ningún magic number en `src/` (verificar con `grep -rn '[0-9]{2,}' src/`)
- [ ] Progreso visible en UI durante import (barra + ETA)
- [ ] `cmake --install` funciona y produce estructura de directorios correcta

---

## Apéndice A: Glosario

| Término | Definición |
|---------|-----------|
| **Bethe-Bloch** | Fórmula de pérdida de energía por ionización para partículas cargadas pesadas (PDG 2022, ec. 34.5) |
| **Sternheimer** | Corrección de densidad al stopping power. Reduce -dE/dx en ~5-15% a altas energías (βγ > 3) |
| **MCS (Multiple Coulomb Scattering)** | Dispersión angular por múltiples colisiones elásticas con núcleos. Fórmula de Highland (1975) |
| **Straggling** | Fluctuación estadística de la pérdida de energía. Modelos: Gaussiano (rápido, ±15%), Vavilov (preciso, ±3%) |
| **OOM** | Out Of Memory — el proceso excede la RAM disponible y es eliminado por el kernel |
| **Sample** | Un punto en una trayectoria: posición (x,y,z), energía depositada, energía cinética, momento, tiempo |
| **Trajectory** | Secuencia ordenada de samples que pertenecen al mismo (eventID, trackID) |
| **Batch** | Subconjunto contiguo de samples procesados juntos (típicamente 100k) |
| **Axial binning** | División del eje longitudinal (z) en intervalos para estadísticas por profundidad |

## Apéndice B: Referencias

- PDG 2022, Chapter 34: "Passage of Particles Through Matter" — Bethe-Bloch formula
- Sternheimer, Berger & Seltzer (1984): "Density Effect for the Ionization Loss of Charged Particles"
- Highland (1975): "Some Practical Remarks on Multiple Scattering" — MCS formula
- Lynch & Dahl (1991): "Approximations to Multiple Coulomb Scattering"
- ICRU Report 49 (1993): "Stopping Powers and Ranges for Protons and Alpha Particles"
- ICRU Report 44 (1989): "Tissue Substitutes in Radiation Dosimetry and Measurement"
- NIST PSTAR: https://physics.nist.gov/PhysRefData/Star/Text/PSTAR.html
- SQLite Amalgamation: https://www.sqlite.org/amalgamation.html
- nlohmann/json: https://github.com/nlohmann/json

## Apéndice C: Notas de Implementación

### C.1 Integración de SQLite amalgamation en CMake

```cmake
FetchContent_Declare(
    sqlite3
    URL https://www.sqlite.org/2024/sqlite-amalgamation-3450000.zip
    URL_HASH SHA256=...
)
FetchContent_MakeAvailable(sqlite3)

add_library(sqlite3 STATIC
    ${sqlite3_SOURCE_DIR}/sqlite3.c
)
target_include_directories(sqlite3 PUBLIC ${sqlite3_SOURCE_DIR})
target_compile_definitions(sqlite3 PRIVATE SQLITE_ENABLE_FTS5)
```

### C.2 Inserción batch eficiente en SQLite

```cpp
void SqliteStorage::flush() {
    sqlite3_exec(db_, "BEGIN TRANSACTION", nullptr, nullptr, nullptr);
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_,
        "INSERT INTO samples VALUES (NULL,?,?,?,?,?,?,?,?,?,?,?)",
        -1, &stmt, nullptr);
    for (auto& sample : pendingBatch_) {
        sqlite3_bind_int64(stmt, 1, sample.trajectoryId);
        // ... bind rest ...
        sqlite3_step(stmt);
        sqlite3_reset(stmt);
    }
    sqlite3_finalize(stmt);
    sqlite3_exec(db_, "COMMIT", nullptr, nullptr, nullptr);
    pendingBatch_.clear();
}
```

### C.3 Factory de Storage basada en tamaño

```cpp
std::unique_ptr<ISampleStorage> ISampleStorage::create(uint64_t estimatedBytes) {
    constexpr uint64_t kThreshold = 100ULL * 1024 * 1024; // 100 MB
    if (estimatedBytes < kThreshold) {
        return std::make_unique<InMemoryStorage>();
    }
    auto dbPath = std::filesystem::temp_directory_path() / "beamlab_samples.db";
    return std::make_unique<SqliteStorage>(dbPath.string());
}
```

### C.4 Feature flag para nuevo pipeline

```cpp
// En config/default_analysis.json:
// "pipeline": { "use_native": true }

// En MainWindow:
if (config_.pipeline.use_native) {
    analysisPresenter_->runNative(inputPath);
} else {
    openDataFileAndRunWithPath(inputPath);  // fallback bash
}
```
