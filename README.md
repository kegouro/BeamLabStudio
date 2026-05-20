<div align="center">

# ⚡ BeamLabStudio

### Physics tools built by a physicist, for physicists

[![Version](https://img.shields.io/badge/version-0.1.0-blue?style=for-the-badge)](https://github.com/kegouro/BeamLabStudio/releases)
[![C++17](https://img.shields.io/badge/C++-17%2B-00599C?style=for-the-badge&logo=c%2B%2B&logoColor=white)](https://en.cppreference.com/w/cpp/17)
[![Qt6](https://img.shields.io/badge/Qt-6-41CD52?style=for-the-badge&logo=qt&logoColor=white)](https://www.qt.io/)
[![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20Windows-lightgrey?style=for-the-badge)](https://github.com/kegouro/BeamLabStudio)
[![License](https://img.shields.io/badge/license-MIT-green?style=for-the-badge)](LICENSE)
[![CI](https://img.shields.io/badge/CI-passing-brightgreen?style=for-the-badge&logo=githubactions&logoColor=white)](https://github.com/kegouro/BeamLabStudio/actions)
[![Tests](https://img.shields.io/badge/tests-41%2F41-brightgreen?style=for-the-badge)](https://github.com/kegouro/BeamLabStudio/actions)

[English](#english) · [Español](#español) · [Releases](https://github.com/kegouro/BeamLabStudio/releases) · [MuonSimViewer](https://github.com/kegouro/MuonSimViewer)

</div>

---

> **Nota del autor:** Estudio Física en la Universidad Técnica Federico Santa María (Chile), no Computer Science.
> BeamLabStudio fue construido entre exámenes de Mecánica Cuántica II, preparación del IELTS C1, y mi
> investigación en el Centro Científico Tecnológico de Valparaíso (CCTVal). En octubre 2026 comienzo
> mi estadía en KIT (Karlsruhe Institute of Technology, Alemania). **PRs son bienvenidos** — si encuentras
> un bug, por favor arréglalo. Probablemente ya esté en Alemania cuando leas esto.
>
> — José Labarca

> **⚠️ MANIFIESTO:** Este proyecto está bajo licencia MIT. **El uso comercial está ESTRICTAMENTE PROHIBIDO
> sin autorización escrita del autor.** Esto incluye forks, derivados, y servicios que usen este código
> con fines de lucro. MuonSimViewer (el predecesor) está bajo la misma restricción. BeamLabStudio fue
> construido sin financiamiento, durante temporada de exámenes, por convicción científica. Si quieres
> usarlo comercialmente, escríbeme.

---

<a name="english"></a>
## 🇬🇧 English

### What is BeamLabStudio?

BeamLabStudio is a scientific workstation for **muon beam trajectory analysis** — the computational
backbone of muon-based cancer therapy research. It ingests Geant4 Monte Carlo stepping data
(CSV/TSV, 15-column format) and produces a complete analysis pipeline: statistical frames,
beam focus detection, 3D envelope surfaces, OBJ geometry exports, and publication-ready
visualizations (PNG, MP4, PDF).

It is the professional rewrite of [MuonSimViewer](https://github.com/kegouro/MuonSimViewer),
a weekend hack that grew into a real research tool.

| Input | Description |
|-------|-------------|
| Geant4 CSV/TSV | 15-column stepping data (x_cm, y_cm, z_cm, edep_MeV, kinE_MeV, momentum, time, track/event IDs, PDG code, particle name, source file) |
| COMSOL CSV | Electromagnetic field data |
| CERN ROOT | Planned (native ROOT importer behind compile flag) |

| Output | Format |
|--------|--------|
| 3D geometry | OBJ (beam envelope, lens disk, trajectory meshes) |
| 2D plots | PNG (beam trajectories, focal slice, envelope proxy) |
| Video | MP4 (120-frame trajectory lambda sweep) |
| Statistics | PDF report + CSV tables |
| Energy profile | CSV (energy_step_profile, track_summary, scoring_planes) |
| Quality report | Markdown + CSV (issues, warnings, physics model limitations) |

<details>
<summary><b>🚀 Big Data Streaming</b> — 20GB+ CSVs without OOM</summary>

BeamLabStudio processes Geant4 CSV files of **any size** with bounded RAM:

- **`importStreaming()`** writes samples directly to `ISampleStorage` — never accumulates in RAM
- **`SqliteStorage`** backend with WAL journal mode, 64 MB page cache, 50k-row batch inserts
- **`BatchStatisticsEngine`** reads from storage in 100k-sample chunks
- **`ISampleStorage::create(fileSize)`** auto-selects InMemory (<100MB) or SQLite (>=100MB)
- Real-time progress: bytes processed / total bytes with ETA
- Cancellation via `std::atomic<bool>` checked between batches

</details>

<details>
<summary><b>🔬 Physics Pipeline</b> — from raw data to publication figures</summary>

```
Geant4 CSV → importStreaming() → ISampleStorage (SQLite)
    → BatchStatisticsEngine (axial frames)
    → FocusDetector (beam waist)
    → SurfaceBuilder (3D meshes) + FullEnvelopePreviewBuilder
    → ExportManager (OBJ, PNG, MP4, PDF, CSV)
    → visualization_manifest.json → BeamLabStudio UI
```

All engines process data in batches. No engine holds the entire dataset in memory.

</details>

<details>
<summary><b>🫁 BioSim</b> — correct physics from day one</summary>

The physics layer uses **StoppingPowerEngine** with:
- **Bethe-Bloch** with **Sternheimer density correction** (not the uncorrected version)
- **Multiple Coulomb Scattering** (Highland-Lynch-Dahl formula)
- **Vavilov/Landau energy straggling** (not just Gaussian)
- **Bortfeld Bragg peak model**
- **BioMaterialLibrary**: 55+ ICRU-44 materials with Sternheimer parameters
- **ParticleLibrary**: 18 particle species from PDG 2022

The old `BetheBlochEngine` (without Sternheimer) has been **deleted**. Only the correct
physics remains.

</details>

<details>
<summary><b>🎨 Qt6 UI</b> — dark theme, High-DPI, no VTK</summary>

- **Single stylesheet**: `src/ui/qt/styles/beamlabstudio.qss` — unique dark theme
- **QPainter 3D**: custom software renderer, zero VTK dependency
- **High-DPI**: enabled by default (`Qt::AA_EnableHighDpiScaling`)
- **Real-time dashboards**: statistics, focus, quality, BioSim energy color maps
- **Drag-and-drop**: drop a CSV onto the window to open
- **Recent files** menu, settings persistence

</details>

<details>
<summary><b>📤 Exports</b> — everything for your paper</summary>

| Format | What | Engine |
|--------|------|--------|
| OBJ | 3D meshes | `SurfaceBuilder`, `FullEnvelopePreviewBuilder` |
| PNG | 2D plots (1900×1300px) | `QGraphicsScene::render()` |
| MP4 | 120-frame trajectory sweep | ffmpeg (optional) |
| PDF | Statistics report | `StatsDashboardWidget` |
| CSV | Energy profile, track summary, scoring planes | `EnergyProfileExporter` |
| Markdown | Analysis summary + quality report | `AnalysisReportExporter`, `QualityReportExporter` |
| JSON | Visualization manifest | `VisualizationManifestExporter` |

</details>

<details>
<summary><b>🖥️ CLI</b> — headless automation</summary>

```bash
# Build
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# Run analysis
./build/beamlab --input stepping_data.csv --output outputs/my_run

# With custom parameters
./build/beamlab --input data.csv --output out \
    --axis z --reference-mode axial-bins --binning uniform --axial-bins 501 --window 5
```

</details>

### Architecture

```
CSV (20 GB)                                          OUTPUTS
    │                                                    ▲
    ▼                                                    │
Geant4CsvImporter::importStreaming()                     │
    │ (line-by-line, O(1) RAM)                           │
    ▼                                                    │
ISampleStorage ◄── InMemoryStorage (<100 MB)            │
              ◄── SqliteStorage (>=100 MB, WAL mode)    │
    │                                                    │
    ▼                                                    │
AnalysisPipeline                                         │
    ├── BatchStatisticsEngine (100k batches)             │
    ├── FocusDetector                                    │
    ├── SurfaceBuilder                                   │
    └── FullEnvelopePreviewBuilder                       │
    │                                                    │
    ▼                                                    │
ExportManager ───────────────────────────────────────────┘
    ├── OBJ  (3D meshes)
    ├── PNG  (2D plots)
    ├── MP4  (video, ffmpeg)
    ├── PDF  (statistics)
    ├── CSV  (energy profiles)
    └── JSON (manifest)
```

### Output Directory Structure

```
outputs/ui_run_dataset_20260520/
├── visualization/
│   ├── trajectories_preview.csv       ← downsampled trajectory samples
│   ├── trajectories_preview.obj       ← 3D trajectory polylines
│   ├── focal_slice_points.csv         ← transverse beam slice data
│   ├── envelope_rings.csv             ← angular envelope proxy rings
│   ├── beam_caustic_preview.obj       ← 3D beam envelope surface
│   ├── effective_lens_disk.obj        ← 3D effective lens mesh
│   └── visualization_manifest.json    ← manifest for UI loading
├── tables/
│   ├── energy_step_profile.csv        ← per-step energy deposition
│   ├── energy_track_summary.csv       ← per-track energy summary
│   ├── scoring_planes.csv             ← detected scoring plane crossings
│   ├── envelope_summary.csv           ← per-slice envelope stats
│   ├── focus_curve.csv                ← focus metric vs axial position
│   └── statistics/
│       └── beam_radius_profile.csv    ← RMS beam radius per axial bin
├── geometry/
│   ├── beam_caustic_surface.obj       ← full-resolution caustic surface
│   ├── effective_lens_disk.obj        ← full-resolution lens disk
│   └── full_beam_envelope_preview.obj ← complete beam envelope
├── reports/
│   ├── analysis_summary.md            ← human-readable analysis summary
│   ├── quality_report.md              ← data quality issues & warnings
│   ├── quality_report.csv             ← machine-readable quality report
│   └── run_metadata.json              ← input/output provenance
├── plots_png/
│   ├── beam_trajectories_z_vs_x.png   ← 2D scatter plot
│   ├── focal_slice_u_vs_v.png         ← transverse beam at focal plane
│   ├── focal_envelope_proxy_u_vs_v.png← envelope proxy at focal plane
│   └── statistics/                    ← auto-generated stat charts
├── csv/
│   ├── tables/                        ← copy of tables/
│   ├── visualization/                 ← copy of visualization CSVs
│   └── statistics/
│       └── beam_radius_profile.csv    ← statistics spreadsheet
├── models_3d/
│   ├── beam_caustic_surface.obj       ← copy of geometry OBJs
│   ├── effective_lens_disk.obj
│   ├── full_beam_envelope_preview.obj
│   └── physical_beamline_geometry.obj ← imported physical beamline (if any)
├── equations/                         ← parametric equations (TXT + CSV)
│   ├── beam_caustic_parametric.txt
│   ├── beam_caustic_parametric.csv
│   ├── effective_lens_disk_parametric.txt
│   └── effective_lens_disk_parametric.csv
├── manifest/
│   └── visualization_manifest.json    ← copy of manifest
└── trajectory_lambda_sweep.mp4        ← 120-frame trajectory animation
```

### Installation

<details>
<summary><b>🐧 Fedora</b></summary>

```bash
sudo dnf install cmake ninja-build gcc-c++ qt6-qtbase-devel mesa-libGL-devel
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBEAMLAB_ENABLE_QT_UI=ON
cmake --build build -j$(nproc)
./build/beamlab_ui
```
</details>

<details>
<summary><b>🐧 Ubuntu 24.04+</b></summary>

```bash
sudo apt install cmake ninja-build g++ qt6-base-dev libgl1-mesa-dev
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBEAMLAB_ENABLE_QT_UI=ON
cmake --build build -j$(nproc)
./build/beamlab_ui
```
</details>

<details>
<summary><b>🪟 Windows</b></summary>

Install Qt6 via the online installer. Build with Visual Studio 2022 + Ninja:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBEAMLAB_ENABLE_QT_UI=ON
cmake --build build -j$env:NUMBER_OF_PROCESSORS
.\build\beamlab_ui.exe
```

**⚠️ Do not build inside OneDrive/Dropbox folders** — CMake will fail with path length limits.
</details>

### Install system-wide

```bash
cmake --install build --prefix /usr/local
```

### Testing

[![Tests](https://img.shields.io/badge/tests-41%2F41-brightgreen?style=for-the-badge)](https://github.com/kegouro/BeamLabStudio/actions)

```bash
cmake -S . -B build -DBEAMLAB_ENABLE_QT_UI=ON
cmake --build build --target beamlab_tests -j$(nproc)
ctest --test-dir build --output-on-failure
```

**Tested modules:**
- `FrameStatisticsEngine` — known moments, empty/single datasets
- `BatchStatisticsEngine` — matches original engine (±1e-12)
- `FocusDetector` — known parabola minimum, edge cases
- `Geant4CsvImporter` — 15-column TSV import, streaming
- `StoppingPowerEngine` — muon in water dEdx, energy clamp, MCS, dose
- `SurfaceBuilder` — vertex/face counts, edge cases
- `ISampleStorage` — 17 tests × 2 backends (InMemory + Sqlite)

### Roadmap

- [x] Core engine (import → statistics → focus → surfaces → export)
- [x] Qt6 GUI with dark theme and High-DPI
- [x] Exports: OBJ, PNG, MP4 (ffmpeg), PDF, CSV, Markdown
- [x] BioSim: StoppingPowerEngine with Sternheimer + MCS + Vavilov
- [x] Big Data: SQLite streaming backend, O(1) RAM for 20GB+ CSVs
- [x] Native analysis pipeline (no bash/QProcess)
- [x] Unit tests (GoogleTest, 41/41, ctest)
- [x] GitHub Actions CI (Build & Test on push/PR)
- [ ] Code signing for Windows
- [ ] AppImage for Linux
- [ ] CERN ROOT native importer (`-DBEAMLAB_ENABLE_ROOT=ON`)
- [ ] GPU-accelerated 3D (compute shaders, not visualization)
- [ ] Clinical dosimetry validation (NIST PSTAR comparison suite)

---

<a name="español"></a>
## 🇨🇱 Español

### ¿Qué es BeamLabStudio?

BeamLabStudio es una estación de trabajo científica para **análisis de trayectorias de haces de
muones** — la columna vertebral computacional de la investigación en terapia contra el cáncer
con muones. Lee datos de stepping de simulaciones Monte Carlo Geant4 (CSV/TSV, 15 columnas)
y produce un pipeline completo de análisis: marcos estadísticos, detección de foco del haz,
superficies 3D de envolvente, exportación de geometría OBJ y visualizaciones listas para
publicación (PNG, MP4, PDF).

Es la reescritura profesional de [MuonSimViewer](https://github.com/kegouro/MuonSimViewer),
un hack de fin de semana que creció hasta convertirse en una herramienta de investigación real.

### Killer Features

- **🚀 Big Data Streaming**: CSVs de 20GB+ sin OOM (SQLite WAL, O(1) RAM, progreso real)
- **🔬 Pipeline de Física**: Estadísticas → Foco → Superficies → Exportación
- **🫁 BioSim**: StoppingPowerEngine con Sternheimer + MCS + Vavilov, 55+ materiales
- **🎨 UI Qt6**: Tema oscuro, High-DPI, visor 3D, dashboards en tiempo real
- **📤 Exportaciones**: OBJ, PNG, MP4, PDF, CSV
- **🖥️ CLI**: Automatización headless

### Instalación

```bash
# Fedora
sudo dnf install cmake ninja-build gcc-c++ qt6-qtbase-devel mesa-libGL-devel

# Ubuntu 24.04+
sudo apt install cmake ninja-build g++ qt6-base-dev libgl1-mesa-dev

# Build
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBEAMLAB_ENABLE_QT_UI=ON
cmake --build build -j$(nproc)
./build/beamlab_ui
```

---

## Créditos

**Autor:** José Labarca — Estudiante de Física, Universidad Santa María (Chile), CCTVal

**Supervisor:** Dr. Claudio Dib — CCTVal, USM

**Predecesor:** [MuonSimViewer](https://github.com/kegouro/MuonSimViewer) — the weekend hack that started it all

**Agradecimientos:** Al equipo CCTVal, a los que respondieron mis preguntas en StackOverflow,
y a Claude (Anthropic) por la asistencia arquitectónica en el refactor.

> *"If you want to go fast, go alone. If you want to go far, go together."*
> — Proverbio africano

---

<div align="center">

### See you at KIT, October 2026 — José

*BeamLabStudio · MIT License · Built unpaid during exam season*

</div>
