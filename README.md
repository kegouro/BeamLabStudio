<div align="center">

# ⚡ BeamLabStudio

### *Physics tools built by a physicist, for physicists*

<br>

[![Version](https://img.shields.io/badge/version-0.2.0-6C63FF?style=for-the-badge)](https://github.com/kegouro/BeamLabStudio/releases)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?style=for-the-badge&logo=cplusplus)](https://en.cppreference.com/w/cpp/20)
[![Qt 6](https://img.shields.io/badge/Qt-6-41CD52?style=for-the-badge&logo=qt)](https://www.qt.io/)
[![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20Windows-lightgrey?style=for-the-badge&logo=linux)](https://github.com/kegouro/BeamLabStudio)
[![License: MIT](https://img.shields.io/badge/license-MIT-yellow?style=for-the-badge)](LICENSE)
[![Status](https://img.shields.io/badge/status-stable-success?style=for-the-badge)](https://github.com/kegouro/BeamLabStudio/releases)

<br>

**Cross-platform scientific platform for beam trajectory analysis, optical surface reconstruction,**
**Monte Carlo post-processing, and biological tissue dosimetry.**

<br>

[English](#english) · [Español](#español) · [Releases](https://github.com/kegouro/BeamLabStudio/releases) · [MuonSimViewer (predecessor)](https://github.com/kegouro/MuonSimViewer)

</div>

---

<a name="english"></a>

## 👤 Author's Note

> I study Physics (4th year, Universidad Santa María — Chile). Not Computer Science.
>
> I built v0.1.0 in roughly **two weeks** while preparing for exams, taking my IELTS (got C1), and surviving a Quantum Mechanics II test that went... let's just say "character building". v0.2.0 adds a real biological simulator: Bethe-Bloch energy loss through tissue, muon tracking, scoring planes. Also built between exams.
>
> If you find a bug, please fix it — I'm probably at **KIT in Germany** by the time you read this. Pull requests are extremely welcome.
>
> — José

**BeamLabStudio** is the full professional rewrite of [MuonSimViewer](https://github.com/kegouro/MuonSimViewer).
MuonSimViewer was the weekend hack. This is the version you can actually use in a real lab.

---

## ⚠️ The Manifesto

This software was created **for science, by a physicist tired of bad tools**.
It exists for students, researchers, and anyone working on muon beam therapy for cancer — at CCTVal-USM or anywhere on Earth.

> **COMMERCIAL USE, SALE, OR MONETIZATION IS STRICTLY PROHIBITED**
> without my explicit written permission — including all forks, derivatives, and spiritual successors (MuonSimViewer included).

I built this unpaid, during exam season. If you want to use it commercially, talk to me first.
I'm not against making money. I'm against people making money off my all-nighters and QM II stress.

---

## 🔬 What is BeamLabStudio?

A complete, self-contained scientific computing platform that takes raw particle trajectory data — from **Geant4**, **COMSOL**, or **CERN ROOT** — and turns it into:

| Output | Description |
|--------|-------------|
| 🎨 3D Visualizations | Interactive beam rendering with VTK + time slider |
| 📊 Beam Physics Metrics | Focus position, RMS radius, envelope area, efficiency |
| 🫁 Biological Dosimetry | *(New)* Bethe-Bloch energy loss through tissue |
| 🧊 3D Meshes | Caustic surfaces, effective lens bodies (OBJ, 3D-printable) |
| 📄 Reports | Multi-page PDF with embedded figures |
| 🎬 Videos | MP4 animation of beam evolution (via ffmpeg) |
| 📐 Equations | Parametric surface descriptions in `.txt` + sampled `.csv` |

---

## 🆕 What's New in v0.2.0

<details>
<summary><b>🫁 Biological Tissue Simulator</b> — click to expand</summary>

<br>

The centerpiece of this update. BeamLabStudio now simulates muon beam propagation through biological tissue using the **Bethe-Bloch stopping power formula**.

| Component | Description |
|-----------|-------------|
| `BetheBlochEngine` | Mean energy loss per unit length for muons in any material |
| `MuonTrackSimulator` | Propagates tracks through arbitrary tissue slab sequences |
| `TissueRegistry` | Configurable library: muscle, bone, water, fat — with physical properties |
| `ScoringPlaneDetector` | Records fluence, energy spectrum, and dose at arbitrary planes |

</details>

<details>
<summary><b>🖥️ New UI Tabs</b> — click to expand</summary>

<br>

- **Simulator tab** (`SimulatorWidget`): configure tissue geometry, run simulations, visualize energy deposition profiles in real time — all inside the GUI
- **Math docs tab** (`InfoWidget`): every formula used in the pipeline, rendered as HTML — no more digging through source code to understand what r_rms means

</details>

<details>
<summary><b>📦 New Export + Bug Fixes</b> — click to expand</summary>

<br>

**New:**
- `EnergyProfileExporter`: exports the full longitudinal energy deposition curve (Bragg peak) to CSV

**Fixed:**
- Numerical instability in `FocusDetector` near degenerate beam configurations
- Edge cases in `SliceExtractor` for very sparse time steps
- Accumulation bug in `FrameStatisticsEngine` when particles are lost before the first frame
- `LensDiskBuilder` generating non-manifold meshes on certain envelope geometries
- `Geant4CsvImporter` failing on files with inconsistent column counts
- `AxisFrameResolver` selecting the wrong propagation axis for near-diagonal beams
- `DatasetNormalizer` applying the wrong scale factor when input units are ambiguous

</details>

---

## ✨ Key Features

### 1 · Intelligent Multi-Format Import

| Format | Details |
|--------|---------|
| **COMSOL CSV** | Auto-detects wide (`@t=...` columns) and long format. Handles Spanish-accented headers. |
| **Geant4 CSV** | Full support. Auto-converts cm→m and ns→s. |
| **CERN ROOT** | Native on Linux (optional build flag). Windows: convert to CSV first. |

### 2 · Complete Physics Analysis Pipeline

```
Raw CSV
  └─► FrameStatisticsEngine
        └─► FocusCurveBuilder ──► FocusDetector
              └─► SliceExtractor
                    └─► ConvexHullEnvelopeExtractor
                          └─► SurfaceBuilder ──► LensDiskBuilder
                                └─► Export (OBJ / PDF / MP4 / CSV)
```

- Automatic axis frame detection or manual override (`--axis x/y/z`)
- Focus detection via RMS radius, convex hull area, or particle count
- Caustic surface reconstruction (parametric + mesh)
- Effective lens body generation — lofted with realistic thickness
- Frame-by-frame statistics: active, arrived, lost, efficiency, r_rms, σ_x, σ_z

### 3 · Biological Simulator

```
Tissue Config
  └─► TissueRegistry + TissueSlab
        └─► BetheBlochEngine
              └─► MuonTrackSimulator
                    └─► ScoringPlaneDetector
                          └─► EnergyProfileExporter (Bragg peak CSV)
```

### 4 · Professional Qt6 GUI

- Dark theme — optimized for long sessions
- Interactive 3D VTK viewer with time slider
- Real-time dashboards: focus curve, envelope area, radial distribution
- **Simulator tab** with live energy deposition plots
- **Math docs tab** with all formulas rendered in HTML
- One-click export: PDF · MP4 · OBJ · CSV · PNG

### 5 · Headless CLI for Automation

```bash
# Basic
beamlab data/run.csv --output outputs/run_001

# Full options
beamlab data/geant4_muons.csv \
  --output results/muon_therapy_v3 \
  --axis z \
  --reference-mode axial-bins \
  --axial-bins 501 \
  --window 7 \
  --caustic-points 128 \
  --lens-points 256 \
  --lens-layers 32 \
  --preview-trajectories 5000
```

### 6 · Bit-Exact Reproducibility

The original Windows post-processing used PowerShell and produced **geometrically different** lens bodies than the Python reference. I rewrote every single line in pure C++20 — same floating-point operations, same iteration order, same output formatting (`%.12e`).

**`diff` between Linux and Windows outputs: zero differences.**

---

## 📦 Output Directory Structure

```
outputs/ui_run_<filename>_<timestamp>/
│
├── geometry/
│   ├── beam_caustic_surface.obj          ← full resolution caustic mesh
│   ├── beam_caustic_surface_preview.obj  ← decimated for fast viewing
│   ├── effective_lens_disk.obj
│   ├── effective_lens_body.obj           ← 3D printable
│   └── trajectories_preview.obj
│
├── visualization/
│   ├── visualization_manifest.json       ← index of every output file
│   ├── trajectories_preview.csv
│   ├── focal_slice_points.csv
│   └── envelope_rings.csv
│
├── plots/                                ← PNG figures (ready for papers)
│
├── reports/
│   ├── analysis_summary.md
│   ├── quality_report.md
│   ├── run_metadata.json
│   └── BeamLabStudio_Report.pdf          ← multi-page, embedded images
│
├── equations/
│   ├── beam_caustic_parametric_equation.txt
│   └── effective_lens_body_parametric_equation.txt
│
├── tables/
│   ├── focus_curve.csv
│   ├── envelope_summary.csv
│   └── energy_profile.csv               ← [New] Bragg peak data
│
└── logs/
```

---

## 🛠️ Installation & Building

### Windows

| Option | Steps |
|--------|-------|
| **Installer** *(recommended)* | Download `BeamLabStudio_Setup_Windows_x64.exe` from Releases → double-click → Next → Install |
| **Portable** | Download `BeamLabStudio_Portable_Windows_x64.exe` → extract anywhere → run |

> ⚠️ **OneDrive users**: do NOT run the installer from a OneDrive-synced folder. OneDrive sometimes leaves large `.exe` files as unsynced placeholders → "Installer integrity check has failed". Move to `C:\Downloads\` first.

### Linux

**Fedora / RHEL**
```bash
sudo dnf install cmake ninja gcc-c++ qt6-qtbase-devel qt6-qtcharts-devel \
                 vtk-devel nlohmann-json-devel
```

**Ubuntu / Debian**
```bash
sudo apt install cmake ninja-build g++ qt6-base-dev qt6-charts-dev \
                 libvtk9-dev nlohmann-json3-dev
```

**Build**
```bash
git clone https://github.com/kegouro/BeamLabStudio.git
cd BeamLabStudio

# Standard (no ROOT)
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# With CERN ROOT (optional, Linux only)
cmake -S . -B build-root -G Ninja -DBEAMLAB_ENABLE_ROOT=ON
cmake --build build-root -j$(nproc)

./build/beamlab --help
./build/beamlab_ui
```

---

## 🏗️ Architecture (If the Repo Dies)

| Layer | Components |
|-------|-----------|
| **Data model** | `Trajectory`, `TrajectorySample`, `TrajectoryDataset`, `AxisFrame`, `BeamEnvelope`, `LensSurfaceModel`, `ParticleInfo` |
| **Import** | `ComsolCsvImporter`, `Geant4CsvImporter`, `RootNativeImporter` |
| **Analysis** | `FrameStatisticsEngine` → `FocusCurveBuilder` → `FocusDetector` → `SliceExtractor` → `ConvexHullEnvelopeExtractor` → `SurfaceBuilder` → `LensDiskBuilder` |
| **Simulation** | `BetheBlochEngine` → `MuonTrackSimulator` → `TissueRegistry` / `TissueSlab` → `ScoringPlaneDetector` |
| **UI** | `MainWindow`, `Scene3DWidget`, `StatsDashboardWidget`, `SimulatorWidget`, `InfoWidget`, `ObjViewerWidget` |
| **Export** | `MeshExporter`, `AnalysisReportExporter`, `VisualizationDataExporter`, `ParametricSurfaceExporter`, `EnergyProfileExporter` |

---

## 🗺️ Roadmap

- [x] Core analysis engine — Linux + Windows bit-equivalent
- [x] Qt6 GUI with 3D VTK viewer
- [x] PDF + MP4 + OBJ export
- [x] Biological tissue simulator (Bethe-Bloch + muon tracking)
- [x] Interactive simulator UI tab
- [x] Math documentation tab
- [x] Energy profile (Bragg peak) export
- [ ] GitHub Actions CI — auto-build releases on tag (Linux + Windows matrix)
- [ ] Code signing for Windows (eliminate SmartScreen warning)
- [ ] AppImage for Linux
- [ ] CERN ROOT support in Windows build
- [ ] Automated bit-equivalence regression tests in CI
- [ ] Full Landau straggling in simulator (currently Gaussian approximation)

---

## 📜 License

MIT — see [LICENSE](LICENSE) for details.

**Extra clause**: commercial use, sale, or any form of monetization of this software or any derivative — including [MuonSimViewer](https://github.com/kegouro/MuonSimViewer) — is **strictly prohibited** without explicit written permission from the author.

---

## 🙏 Credits

- **Author & Architect**: José Labarca — Universidad Santa María / CCTVal, muon beam cancer therapy project
- **Dependencies**: Qt 6 (LGPL) · VTK (BSD) · nlohmann/json (MIT) · CERN ROOT (LGPL) · MinGW-w64 · NSIS (zlib)

---

<div align="center">

**If you use BeamLabStudio in your research, please cite it and drop me an email.**
*I would love to know this thing I built during exam hell is actually helping real science.*

<br>

**See you at KIT, October 2026.**

*— José*

</div>

---

---
---

<a name="español"></a>

<div align="center">

# ⚡ BeamLabStudio

### *Herramientas de física hechas por un físico, para físicos*

<br>

[![Versión](https://img.shields.io/badge/versión-0.2.0-6C63FF?style=for-the-badge)](https://github.com/kegouro/BeamLabStudio/releases)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?style=for-the-badge&logo=cplusplus)](https://en.cppreference.com/w/cpp/20)
[![Qt 6](https://img.shields.io/badge/Qt-6-41CD52?style=for-the-badge&logo=qt)](https://www.qt.io/)
[![Plataforma](https://img.shields.io/badge/plataforma-Linux%20%7C%20Windows-lightgrey?style=for-the-badge&logo=linux)](https://github.com/kegouro/BeamLabStudio)
[![Licencia: MIT](https://img.shields.io/badge/licencia-MIT-yellow?style=for-the-badge)](LICENSE)
[![Estado](https://img.shields.io/badge/estado-estable-success?style=for-the-badge)](https://github.com/kegouro/BeamLabStudio/releases)

<br>

**Plataforma científica multiplataforma para análisis de trayectorias de haces, reconstrucción de superficies ópticas,**
**post-proceso de simulaciones Monte Carlo y dosimetría en tejido biológico.**

<br>

[English](#english) · [Español](#español) · [Releases](https://github.com/kegouro/BeamLabStudio/releases) · [MuonSimViewer (predecesor)](https://github.com/kegouro/MuonSimViewer)

</div>

---

## 👤 Nota del Autor

> Estudio Licenciatura en Física (4to año, Universidad Santa María — Chile). No Ingeniería Informática.
>
> Hice v0.1.0 en **dos semanas** mientras rendía certámenes, daba el IELTS (saqué C1) y sobrevivía un examen de Cuántica II que fue... digamos que "formador de carácter". v0.2.0 agrega un simulador biológico de verdad: pérdida de energía Bethe-Bloch en tejido, tracking de muones, planos de scoring. También hecho entre exámenes.
>
> Si encontrás un bug, arreglalo por mí — probablemente ya estoy en **KIT en Alemania** cuando leas esto. Los pull requests son extremadamente bienvenidos.
>
> — José

**BeamLabStudio** es la reescritura profesional completa de [MuonSimViewer](https://github.com/kegouro/MuonSimViewer).
MuonSimViewer fue el hack de fin de semana. Esta es la versión que podés usar en un laboratorio real.

---

## ⚠️ El Manifiesto

Este software fue creado **para la ciencia, por un físico harto de herramientas malas**.
Existe para estudiantes, investigadores y cualquiera que trabaje en terapia con muones contra el cáncer — en el CCTVal-USM o en cualquier lugar del planeta.

> **EL USO COMERCIAL, VENTA O MONETIZACIÓN ESTÁN ESTRICTAMENTE PROHIBIDOS**
> sin mi permiso expreso por escrito — incluyendo todos los forks, derivados y sucesores espirituales (MuonSimViewer incluido).

Lo construí sin cobrar, durante época de exámenes. Si querés usarlo comercialmente, hablame primero.
No estoy en contra de hacer plata. Estoy en contra de que alguien haga plata con mis noches en blanco y estrés por Cuántica II.

---

## 🔬 ¿Qué es BeamLabStudio?

Una plataforma completa de computación científica que toma datos crudos de trayectorias de partículas — de **Geant4**, **COMSOL** o **CERN ROOT** — y los convierte en:

| Salida | Descripción |
|--------|-------------|
| 🎨 Visualizaciones 3D | Renderizado interactivo del haz con VTK + slider de tiempo |
| 📊 Métricas de Física | Posición de foco, radio RMS, área de envolvente, eficiencia |
| 🫁 Dosimetría Biológica | *(Nuevo)* Pérdida de energía Bethe-Bloch en tejido |
| 🧊 Mallas 3D | Superficies caústicas, cuerpos de lente efectivos (OBJ, imprimibles en 3D) |
| 📄 Reportes | PDF multi-página con figuras embebidas |
| 🎬 Videos | Animación MP4 de la evolución del haz (vía ffmpeg) |
| 📐 Ecuaciones | Descripciones paramétricas en `.txt` + muestreo en `.csv` |

---

## 🆕 Novedades en v0.2.0

<details>
<summary><b>🫁 Simulador de Tejido Biológico</b> — click para expandir</summary>

<br>

El núcleo de esta actualización. BeamLabStudio ahora simula la propagación de haces de muones a través de tejido biológico usando la **fórmula de Bethe-Bloch**.

| Componente | Descripción |
|-----------|-------------|
| `BetheBlochEngine` | Pérdida de energía media por unidad de longitud para muones en cualquier material |
| `MuonTrackSimulator` | Propaga trayectorias a través de secuencias arbitrarias de tejido |
| `TissueRegistry` | Biblioteca configurable: músculo, hueso, agua, grasa — con propiedades físicas |
| `ScoringPlaneDetector` | Registra fluencia, espectro de energía y dosis en planos arbitrarios |

</details>

<details>
<summary><b>🖥️ Nuevas Pestañas de la Interfaz</b> — click para expandir</summary>

<br>

- **Pestaña Simulador** (`SimulatorWidget`): configurá geometría de tejido, corré simulaciones y visualizá perfiles de deposición de energía en tiempo real — todo desde la GUI
- **Pestaña Explicaciones** (`InfoWidget`): cada fórmula usada en el pipeline, renderizada en HTML — se acabó buscar en el código fuente qué significa r_rms

</details>

<details>
<summary><b>📦 Nueva Exportación + Correcciones</b> — click para expandir</summary>

<br>

**Nuevo:**
- `EnergyProfileExporter`: exporta la curva de deposición de energía longitudinal (pico de Bragg) a CSV

**Corregido:**
- Inestabilidad numérica en `FocusDetector` cerca de configuraciones de haz degeneradas
- Casos extremos en `SliceExtractor` para pasos de tiempo muy dispersos
- Bug de acumulación en `FrameStatisticsEngine` cuando partículas se pierden antes del primer frame
- `LensDiskBuilder` generando mallas no-manifold en ciertas geometrías de envolvente
- `Geant4CsvImporter` fallando en archivos con conteo de columnas inconsistente
- `AxisFrameResolver` seleccionando el eje de propagación incorrecto en haces casi-diagonales
- `DatasetNormalizer` aplicando el factor de escala incorrecto con unidades de entrada ambiguas

</details>

---

## ✨ Características Principales

### 1 · Importación Inteligente Multi-Formato

| Formato | Detalles |
|---------|---------|
| **COMSOL CSV** | Detecta automáticamente formato ancho (`@t=...`) y largo. Maneja encabezados con acentos. |
| **Geant4 CSV** | Soporte completo. Convierte automáticamente cm→m y ns→s. |
| **CERN ROOT** | Nativo en Linux (flag opcional). Windows: convertir a CSV primero. |

### 2 · Pipeline Completo de Análisis Físico

```
CSV crudo
  └─► FrameStatisticsEngine
        └─► FocusCurveBuilder ──► FocusDetector
              └─► SliceExtractor
                    └─► ConvexHullEnvelopeExtractor
                          └─► SurfaceBuilder ──► LensDiskBuilder
                                └─► Export (OBJ / PDF / MP4 / CSV)
```

- Detección automática del sistema de ejes o sobrescritura manual (`--axis x/y/z`)
- Detección de foco: radio RMS, área de casco convexo, conteo de partículas
- Reconstrucción de superficie caústica (paramétrica + malla)
- Cuerpo de lente efectivo con lofting y espesor realista
- Estadísticas frame por frame: activas, llegadas, perdidas, eficiencia, r_rms, σ_x, σ_z

### 3 · Simulador Biológico

```
Configuración de tejido
  └─► TissueRegistry + TissueSlab
        └─► BetheBlochEngine
              └─► MuonTrackSimulator
                    └─► ScoringPlaneDetector
                          └─► EnergyProfileExporter (CSV pico de Bragg)
```

### 4 · Interfaz Gráfica Profesional Qt6

- Tema oscuro — optimizado para sesiones largas
- Visor 3D interactivo con VTK y slider de tiempo
- Dashboards en tiempo real: curva de foco, área de envolvente, distribución radial
- **Pestaña Simulador** con gráficos de deposición de energía en vivo
- **Pestaña Explicaciones** con todas las fórmulas renderizadas en HTML
- Exportación con un clic: PDF · MP4 · OBJ · CSV · PNG

### 5 · CLI Headless para Automatización

```bash
# Básico
beamlab datos/corrida.csv --output outputs/corrida_001

# Opciones completas
beamlab datos/geant4_muones.csv \
  --output resultados/terapia_muones_v3 \
  --axis z \
  --reference-mode axial-bins \
  --axial-bins 501 \
  --window 7 \
  --caustic-points 128 \
  --lens-points 256 \
  --lens-layers 32 \
  --preview-trajectories 5000
```

### 6 · Reproducibilidad Bit-Exacta

El post-proceso original de Windows usaba PowerShell y producía **cuerpos de lente geométricamente diferentes** que la referencia Python. Reescribí cada línea en C++20 puro — mismas operaciones de punto flotante, mismo orden de iteración, mismo formato de salida (`%.12e`).

**`diff` entre salidas de Linux y Windows: cero diferencias.**

---

## 📦 Estructura del Directorio de Salida

```
outputs/ui_run_<archivo>_<timestamp>/
│
├── geometry/
│   ├── beam_caustic_surface.obj          ← malla caústica resolución completa
│   ├── beam_caustic_surface_preview.obj  ← decimada para vista rápida
│   ├── effective_lens_disk.obj
│   ├── effective_lens_body.obj           ← imprimible en 3D
│   └── trajectories_preview.obj
│
├── visualization/
│   ├── visualization_manifest.json       ← índice de todos los archivos de salida
│   ├── trajectories_preview.csv
│   ├── focal_slice_points.csv
│   └── envelope_rings.csv
│
├── plots/                                ← figuras PNG (listas para papers)
│
├── reports/
│   ├── analysis_summary.md
│   ├── quality_report.md
│   ├── run_metadata.json
│   └── BeamLabStudio_Report.pdf          ← multi-página, imágenes embebidas
│
├── equations/
│   ├── beam_caustic_parametric_equation.txt
│   └── effective_lens_body_parametric_equation.txt
│
├── tables/
│   ├── focus_curve.csv
│   ├── envelope_summary.csv
│   └── energy_profile.csv               ← [Nuevo] datos del pico de Bragg
│
└── logs/
```

---

## 🛠️ Instalación y Compilación

### Windows

| Opción | Pasos |
|--------|-------|
| **Instalador** *(recomendado)* | Descargá `BeamLabStudio_Setup_Windows_x64.exe` desde Releases → doble clic → Siguiente → Instalar |
| **Portable** | Descargá `BeamLabStudio_Portable_Windows_x64.exe` → extraé donde quieras → ejecutá |

> ⚠️ **Usuarios de OneDrive**: NO ejecutes el instalador desde una carpeta sincronizada con OneDrive. OneDrive a veces deja archivos `.exe` grandes como placeholders sin sincronizar → "Installer integrity check has failed". Mové el archivo a `C:\Downloads\` primero.

### Linux

**Fedora / RHEL**
```bash
sudo dnf install cmake ninja gcc-c++ qt6-qtbase-devel qt6-qtcharts-devel \
                 vtk-devel nlohmann-json-devel
```

**Ubuntu / Debian**
```bash
sudo apt install cmake ninja-build g++ qt6-base-dev qt6-charts-dev \
                 libvtk9-dev nlohmann-json3-dev
```

**Compilación**
```bash
git clone https://github.com/kegouro/BeamLabStudio.git
cd BeamLabStudio

# Estándar (sin ROOT)
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# Con CERN ROOT (opcional, solo Linux)
cmake -S . -B build-root -G Ninja -DBEAMLAB_ENABLE_ROOT=ON
cmake --build build-root -j$(nproc)

./build/beamlab --help
./build/beamlab_ui
```

---

## 🏗️ Arquitectura (Si el Repo Muere)

| Capa | Componentes |
|------|-----------|
| **Modelo de datos** | `Trajectory`, `TrajectorySample`, `TrajectoryDataset`, `AxisFrame`, `BeamEnvelope`, `LensSurfaceModel`, `ParticleInfo` |
| **Importación** | `ComsolCsvImporter`, `Geant4CsvImporter`, `RootNativeImporter` |
| **Análisis** | `FrameStatisticsEngine` → `FocusCurveBuilder` → `FocusDetector` → `SliceExtractor` → `ConvexHullEnvelopeExtractor` → `SurfaceBuilder` → `LensDiskBuilder` |
| **Simulación** | `BetheBlochEngine` → `MuonTrackSimulator` → `TissueRegistry` / `TissueSlab` → `ScoringPlaneDetector` |
| **UI** | `MainWindow`, `Scene3DWidget`, `StatsDashboardWidget`, `SimulatorWidget`, `InfoWidget`, `ObjViewerWidget` |
| **Exportación** | `MeshExporter`, `AnalysisReportExporter`, `VisualizationDataExporter`, `ParametricSurfaceExporter`, `EnergyProfileExporter` |

---

## 🗺️ Roadmap

- [x] Motor de análisis central — Linux + Windows bit-equivalente
- [x] GUI Qt6 con visor 3D VTK
- [x] Exportación PDF + MP4 + OBJ
- [x] Simulador de tejido biológico (Bethe-Bloch + tracking de muones)
- [x] Pestaña de simulador interactivo
- [x] Pestaña de documentación matemática
- [x] Exportación de perfil de energía (pico de Bragg)
- [ ] GitHub Actions CI — releases automáticos en tag (matrix Linux + Windows)
- [ ] Firma de código para Windows (eliminar advertencia SmartScreen)
- [ ] AppImage para Linux
- [ ] Soporte CERN ROOT en build de Windows
- [ ] Tests de regresión bit-equivalencia Linux vs Windows en CI
- [ ] Straggling de Landau completo en el simulador (actualmente aproximación gaussiana)

---

## 📜 Licencia

MIT — ver [LICENSE](LICENSE) para los detalles.

**Cláusula extra**: el uso comercial, venta o cualquier forma de monetización de este software o cualquier derivado — incluyendo [MuonSimViewer](https://github.com/kegouro/MuonSimViewer) — está **estrictamente prohibido** sin permiso expreso por escrito del autor.

---

## 🙏 Créditos

- **Autor y Arquitecto**: José Labarca — Universidad Santa María / CCTVal (terapia con haces de muones contra el cáncer)
- **Librerías**: Qt 6 (LGPL) · VTK (BSD) · nlohmann/json (MIT) · CERN ROOT (LGPL) · MinGW-w64 · NSIS (zlib)

---

<div align="center">

**Si usás BeamLabStudio en tu investigación, por favor citame y mandame un mail.**
*Me encantaría saber que esta cosa que construí durante el infierno de exámenes está ayudando a ciencia real.*

<br>

**Nos vemos en Karlsruhe, octubre 2026.**

*— José*

</div>

---

<sub>Parte del **[Pharos Project](https://kegouro.github.io)** — infraestructura científica y educativa sin barreras de entrada. · José Labarca Baeza</sub>
