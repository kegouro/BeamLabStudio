# BeamLabStudio v0.2.0

**Cross-platform scientific application for beam trajectory analysis, optical surface reconstruction, Monte Carlo simulation post-processing, and biological tissue dosimetry.**

<p align="center">
  <img src="https://img.shields.io/badge/version-0.2.0-blue" alt="Version 0.2.0">
  <img src="https://img.shields.io/badge/C%2B%2B-20-00599C" alt="C++20">
  <img src="https://img.shields.io/badge/Qt-6-41CD52" alt="Qt 6">
  <img src="https://img.shields.io/badge/platform-Linux%20%7C%20Windows-lightgrey" alt="Platform">
  <img src="https://img.shields.io/badge/status-v0.2.0%20stable-success" alt="Stable">
  <img src="https://img.shields.io/badge/license-MIT-yellow" alt="MIT License">
</p>

---

## Author's Note

> I study Physics (4th year, Universidad Santa María - Chile), not Computer Science.  
> I built v0.1.0 in roughly two weeks while preparing for exams, taking my IELTS (C1), and surviving a Quantum Mechanics II test that went... let's just say "character building".  
> v0.2.0 adds a biological particle simulator — Bethe-Bloch energy loss through tissue slabs, muon tracking, scoring planes. Still built between exams.  
> If you find a bug, please fix it for me — I'm probably at KIT in Germany by the time you read this.  
> The Linux build works perfectly. Windows works too (I cross-compiled it myself from Fedora).  
> Pull requests are extremely welcome.

**This is the full professional rewrite** of my earlier prototype [MuonSimViewer](https://github.com/kegouro/MuonSimViewer).  
MuonSimViewer was the "weekend hack" version. BeamLabStudio is the production-grade, bit-equivalent, cross-platform tool you can actually use in a real research lab.

---

## The Manifesto

This software was created **for science, by a physicist who was tired of bad tools**.  
It exists **for the world** — for students, researchers, national labs, and anyone working on muon beam therapy for cancer at CCTVal-USM or anywhere else on Earth.

**COMMERCIAL USE, SALE, OR ANY FORM OF MONETIZATION IS STRICTLY PROHIBITED**  
without my explicit written permission. This includes BeamLabStudio itself and **all derivatives**, forks, or spiritual successors (including MuonSimViewer).

This is academic and research software. I built it unpaid, during exam season.  
If you want to use it commercially, talk to me first.

---

## What is BeamLabStudio?

BeamLabStudio is a complete, self-contained scientific computing platform that takes raw particle trajectory data (from Geant4, COMSOL, or CERN ROOT) and turns it into:

- Beautiful 3D visualizations
- Quantitative beam physics metrics (focus position, RMS radius, envelope area, efficiency, etc.)
- Reconstructed optical surfaces (effective lens bodies, caustics)
- Publication-ready reports (PDF + CSV + OBJ meshes + parametric equations)
- Animated MP4 videos of the beam evolution
- **[New in v0.2.0]** Biological dosimetry — simulated energy deposition of muon beams through tissue

It was designed specifically for the muon beam cancer therapy project at CCTVal, but it is general enough for any charged particle beam transport problem.

---

## What's New in v0.2.0

### Biological Tissue Simulator (`src/simulation/`)
The centerpiece of this update. BeamLabStudio can now simulate muon beam propagation through biological tissue slabs using the Bethe-Bloch stopping power formula:

- **Bethe-Bloch engine** (`BetheBlochEngine`): computes mean energy loss per unit length for muons in any material
- **Muon track simulator** (`MuonTrackSimulator`): propagates individual tracks through arbitrary sequences of tissue slabs
- **Tissue registry** (`TissueRegistry`): configurable library of biological materials (muscle, bone, water, fat...) with their physical properties
- **Scoring plane detector** (`ScoringPlaneDetector`): records particle fluence, energy spectrum, and dose at arbitrary planes

### New UI Tabs
- **Simulator tab** (`SimulatorWidget`): interactive UI to configure tissue geometry, run simulations, and visualize energy deposition profiles in real time
- **Math docs tab** (`InfoWidget`): rendered HTML documentation of every formula used in the analysis pipeline — no more hunting through source code to understand what r_rms means

### New Export Format
- **Energy profile export** (`EnergyProfileExporter`): exports the full longitudinal energy deposition curve (Bragg peak) to CSV, ready for plotting

### Bug Fixes
- Fixed numerical instability in `FocusDetector` near degenerate beam configurations
- Fixed edge cases in `SliceExtractor` for very sparse time steps
- Fixed `FrameStatisticsEngine` accumulation bug when particles are lost before the first frame
- Fixed `LensDiskBuilder` producing non-manifold meshes on certain envelope geometries
- Fixed `Geant4CsvImporter` failing on files with inconsistent column counts
- Fixed `AxisFrameResolver` selecting the wrong propagation axis for near-diagonal beams
- Fixed `DatasetNormalizer` applying the wrong scale factor when input units are ambiguous

---

## Key Features

### 1. Intelligent Multi-Format Import
- **COMSOL CSV**: Automatically detects both "wide" format (columns with `@ t=...`) and "long" format. No manual column mapping required. Handles Spanish-accented headers (`posición de partícula`) and weird whitespace.
- **Geant4 CSV**: Full support for standard Geant4 output (`x_cm`, `y_cm`, `z_cm`, `time_ns`, `trackID`, `eventID`). Automatically converts cm→m and ns→s.
- **CERN ROOT**: Native support on Linux (optional build flag). On Windows, convert to CSV first.

### 2. Complete Physics Analysis Pipeline
- Automatic axis frame detection or manual override (`--axis x/y/z`)
- Focus detection using multiple metrics (RMS radius, envelope area, particle count)
- Beam envelope calculation with convex hull
- Caustic surface reconstruction (parametric + mesh)
- Effective lens body generation (lofted surfaces with realistic thickness)
- Frame-by-frame statistics (active particles, arrived, lost, efficiency, r_rms, sigma_x, sigma_z)

### 3. Biological Simulator
- Bethe-Bloch energy loss through configurable tissue slab stacks
- Muon track propagation with stochastic energy straggling
- Scoring planes for fluence, energy spectrum, and dose
- Bragg peak visualization and CSV export

### 4. Professional Qt6 Graphical Interface
- Dark theme optimized for long analysis sessions
- Interactive 3D VTK viewer with time slider
- Real-time dashboards (focus curve, envelope area, point count, radial distribution)
- Simulator tab with live energy deposition plots
- Math documentation tab with all formulas rendered in HTML
- One-click export of everything (PDF report, MP4, OBJ, CSV, PNG plots)

### 5. Headless CLI for Automation
```bash
beamlab --input data.csv --output results/run42 --caustic-points 128 --lens-layers 24
```
Perfect for cluster jobs and batch processing.

### 6. Numerically Reproducible
The Windows and Linux versions produce **bit-identical** output (IEEE 754). Every single line of the post-processing pipeline is in pure C++20 with deterministic floating-point operations and output formatting (`%.12e`).

### 7. Rich Export Formats
| Format | Content |
|--------|---------|
| `.obj` | 3D meshes (full resolution + decimated preview) |
| `.csv` | Tables, energy profiles, trajectory previews |
| `.png` | Publication-ready plots |
| `.mp4` | Beam evolution animation (requires ffmpeg) |
| `.pdf` | Multi-page report with embedded images |
| `.txt` | Parametric surface equations |
| `.json` | Run metadata and manifest |

---

## Installation & Building

### Windows (Easiest)

**Option A — Installer (Recommended)**
1. Download `BeamLabStudio_Setup_Windows_x64.exe` from Releases
2. Double-click → Next → Next → Install (no admin rights needed)
3. The app launches automatically when done

**Option B — Portable**
1. Download `BeamLabStudio_Portable_Windows_x64.exe`
2. Extract anywhere (USB stick works)
3. Run `BeamLabStudio.exe`

> **OneDrive Warning**: Do NOT run the installer from a OneDrive-synced folder. OneDrive sometimes leaves large `.exe` files as unsynced placeholders, causing "Installer integrity check has failed". Move the file to `C:\Downloads\` first.

### Linux (Native — Recommended for Development)

**Prerequisites**

Fedora/RHEL:
```bash
sudo dnf install cmake ninja gcc-c++ qt6-qtbase-devel qt6-qtcharts-devel \
                 vtk-devel nlohmann-json-devel
```

Ubuntu/Debian:
```bash
sudo apt install cmake ninja-build g++ qt6-base-dev qt6-charts-dev \
                 libvtk9-dev nlohmann-json3-dev
```

**Build**
```bash
git clone https://github.com/kegouro/BeamLabStudio.git
cd BeamLabStudio

# Standard build
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# With CERN ROOT support (optional)
cmake -S . -B build-root -G Ninja -DBEAMLAB_ENABLE_ROOT=ON
cmake --build build-root -j$(nproc)

# Run
./build/beamlab --help
./build/beamlab_ui
```

**Cross-compiling Windows binaries from Fedora (for maintainers)**  
See cross-compile notes in the repository. Involves MinGW-w64, NSIS, and careful DLL pruning.

---

## Usage Examples

### CLI (Headless)
```bash
# Basic analysis
beamlab data/comsol_run.csv --output outputs/run_2026-05-07

# Advanced options
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

### GUI
```bash
beamlab_ui
# File → Open → select CSV → Analyze → Export everything
# Or use the Simulator tab to run Bethe-Bloch tissue simulations
```

---

## Output Directory Structure

```
outputs/ui_run_<filename>_<timestamp>/
├── geometry/
│   ├── beam_caustic_surface.obj              # Full resolution caustic mesh
│   ├── beam_caustic_surface_preview.obj      # Decimated for fast viewing
│   ├── effective_lens_disk.obj
│   ├── effective_lens_body.obj               # 3D printable
│   └── trajectories_preview.obj
├── visualization/
│   ├── visualization_manifest.json
│   ├── trajectories_preview.csv
│   ├── focal_slice_points.csv
│   └── envelope_rings.csv
├── plots/                                    # PNG figures (ready for papers)
├── reports/
│   ├── analysis_summary.md
│   ├── quality_report.md
│   ├── run_metadata.json
│   └── BeamLabStudio_Report.pdf
├── equations/
│   ├── beam_caustic_parametric_equation.txt
│   └── effective_lens_body_parametric_equation.txt
├── tables/
│   ├── focus_curve.csv
│   ├── envelope_summary.csv
│   └── energy_profile.csv                   # [New] Bragg peak data
└── logs/
```

---

## Technical Deep Dive

### The COMSOL Parser
COMSOL exports trajectory data in two completely different formats:
- **Wide format**: One row per particle, hundreds of columns like `qx @ t=0.1ns`
- **Long format**: One row per (particle, time) pair

BeamLabStudio autodetects which format you have. The parser handles Spanish-accented headers and inconsistent whitespace.

### Focus Detection
Multiple metrics are computed per time step: RMS beam radius, convex hull area, and particle count inside the acceptance aperture. Focus = global minimum of the chosen metric. Override with `--reference-mode`.

### Surface Reconstruction
The effective lens body is generated by lofting the beam envelope at the focal slice with realistic thickness distribution (thicker at center, thinner at edges — like a real magnetic lens).

### Bethe-Bloch Simulator
Energy loss per unit length is computed via the relativistic Bethe-Bloch formula. Tracks are propagated step-by-step through tissue slabs. Stochastic straggling is handled via a Gaussian approximation of the Landau distribution. Scoring planes record full phase-space information at configurable positions.

### Bit-Exact Reproducibility
The original Windows post-processing used PowerShell and produced **geometrically different** lens bodies than the Python reference. Solution: entire pipeline rewritten in pure C++20 with identical floating-point operations, iteration order, and output formatting (`%.12e`). `diff` between Linux and Windows outputs: zero differences.

---

## Architecture (If the Repo Dies)

1. **Core data model**: `Trajectory`, `TrajectorySample`, `TrajectoryDataset`, `AxisFrame`, `BeamEnvelope`, `LensSurfaceModel`, `ParticleInfo`
2. **Import layer**: `ComsolCsvImporter`, `Geant4CsvImporter`, `RootNativeImporter`
3. **Analysis layer**: `FrameStatisticsEngine` → `FocusCurveBuilder` → `FocusDetector` → `SliceExtractor` → `ConvexHullEnvelopeExtractor` → `SurfaceBuilder` → `LensDiskBuilder`
4. **Simulation layer**: `BetheBlochEngine` → `MuonTrackSimulator` → `TissueRegistry` / `TissueSlab` → `ScoringPlaneDetector`
5. **UI layer**: `MainWindow`, `Scene3DWidget`, `StatsDashboardWidget`, `SimulatorWidget`, `InfoWidget`, `ObjViewerWidget`
6. **Export layer**: `MeshExporter`, `AnalysisReportExporter`, `VisualizationDataExporter`, `ParametricSurfaceExporter`, `EnergyProfileExporter`

---

## Roadmap

- [x] Core analysis engine (Linux + Windows bit-equivalent)
- [x] Qt6 GUI with 3D VTK viewer
- [x] PDF + MP4 + OBJ export
- [x] Biological tissue simulator (Bethe-Bloch + muon tracking)
- [x] Interactive simulator UI tab
- [x] Math documentation tab with rendered formulas
- [x] Energy profile (Bragg peak) export
- [ ] GitHub Actions CI: auto-build releases on tag (Linux + Windows cross-compile matrix)
- [ ] Code signing for Windows (eliminate SmartScreen warning)
- [ ] AppImage for Linux
- [ ] CERN ROOT support in Windows build (blocked by cross-compilation complexity)
- [ ] Automated bit-equivalence regression tests in CI
- [ ] Full stochastic Landau straggling in the simulator (currently Gaussian approximation)

---

## License

MIT — see [LICENSE](LICENSE) for details.

**Extra clause**: Commercial use, sale, or any form of monetization of this software or any derivative (including MuonSimViewer) is **strictly prohibited** without explicit written permission from the author.

---

## Credits

- **Author**: José Labarca — Universidad Santa María / CCTVal, muon beam cancer therapy project
- **Dependencies**: Qt 6 (LGPL), VTK (BSD), nlohmann/json (MIT), CERN ROOT (LGPL), MinGW-w64, NSIS (zlib)

---

**If you use BeamLabStudio in your research, please cite it and drop me an email.**  
I would love to know this thing I built during exam hell is actually helping real science.

**See you at KIT, October 2026.**

— José

---

================================================================================
                           VERSIÓN EN ESPAÑOL
================================================================================

# BeamLabStudio v0.2.0

**Aplicación científica multiplataforma para análisis de trayectorias de haces, reconstrucción de superficies ópticas, post-proceso de simulaciones Monte Carlo y dosimetría en tejido biológico.**

<p align="center">
  <img src="https://img.shields.io/badge/version-0.2.0-blue" alt="Version 0.2.0">
  <img src="https://img.shields.io/badge/C%2B%2B-20-00599C" alt="C++20">
  <img src="https://img.shields.io/badge/Qt-6-41CD52" alt="Qt 6">
  <img src="https://img.shields.io/badge/platform-Linux%20%7C%20Windows-lightgrey" alt="Platform">
  <img src="https://img.shields.io/badge/status-v0.2.0%20stable-success" alt="Stable">
  <img src="https://img.shields.io/badge/license-MIT-yellow" alt="MIT License">
</p>

---

## Nota del Autor

> Estudio Licenciatura en Física (4to año, Universidad Santa María - Chile), no Ingeniería Informática.  
> Hice v0.1.0 en dos semanas mientras rendía certámenes, daba el IELTS (saqué C1) y sobrevivía Cuántica II.  
> v0.2.0 agrega un simulador biológico de verdad: pérdida de energía Bethe-Bloch en tejido, tracking de muones, planos de scoring. Sigue siendo hecho entre exámenes.  
> Si encontrás un bug, arreglalo por mí — probablemente ya esté en KIT en Alemania cuando leas esto.  
> Linux funciona perfecto. Windows también (lo cross-compileé desde Fedora).  
> Los pull requests son extremadamente bienvenidos.

**Esta es la reescritura profesional completa** de mi prototipo anterior [MuonSimViewer](https://github.com/kegouro/MuonSimViewer).  
MuonSimViewer fue el "hack de fin de semana". BeamLabStudio es la herramienta de grado de producción que podés usar en un laboratorio de investigación real.

---

## El Manifiesto

Este software fue creado **para la ciencia, por un físico harto de herramientas malas**.  
Existe **para el mundo** — para estudiantes, investigadores, laboratorios nacionales y cualquiera trabajando en terapia con muones contra el cáncer en el CCTVal-USM o en cualquier otro lugar del planeta.

**EL USO COMERCIAL, VENTA O CUALQUIER FORMA DE MONETIZACIÓN ESTÁ ESTRICTAMENTE PROHIBIDA**  
sin mi permiso expreso por escrito. Esto incluye BeamLabStudio y **todos sus derivados**, forks o sucesores espirituales (incluyendo MuonSimViewer).

Lo construí sin cobrar, durante época de exámenes, para la ciencia y para el CCTVal. Mantengámoslo así.

---

## ¿Qué es BeamLabStudio?

BeamLabStudio es una plataforma completa de computación científica que toma datos crudos de trayectorias de partículas (de Geant4, COMSOL o CERN ROOT) y los convierte en:

- Visualizaciones 3D interactivas
- Métricas cuantitativas de física de haces (posición de foco, radio RMS, área de envolvente, eficiencia, etc.)
- Superficies ópticas reconstruidas (cuerpos de lente efectivos, caústicas)
- Reportes listos para publicación (PDF + CSV + mallas OBJ + ecuaciones paramétricas)
- Videos animados MP4 de la evolución del haz
- **[Nuevo en v0.2.0]** Dosimetría biológica — deposición de energía de haces de muones en tejido

---

## Novedades en v0.2.0

### Simulador de Tejido Biológico (`src/simulation/`)
El núcleo de esta actualización. BeamLabStudio ahora puede simular la propagación de haces de muones a través de capas de tejido biológico usando la fórmula de Bethe-Bloch:

- **Motor Bethe-Bloch** (`BetheBlochEngine`): calcula la pérdida de energía media por unidad de longitud para muones en cualquier material
- **Simulador de trayectorias** (`MuonTrackSimulator`): propaga trayectorias individuales a través de secuencias arbitrarias de tejido
- **Registro de tejidos** (`TissueRegistry`): biblioteca configurable de materiales biológicos (músculo, hueso, agua, grasa...) con sus propiedades físicas
- **Detector de planos de scoring** (`ScoringPlaneDetector`): registra fluencia, espectro de energía y dosis en planos arbitrarios

### Nuevas Pestañas de la Interfaz
- **Pestaña Simulador** (`SimulatorWidget`): UI interactiva para configurar geometría de tejido, correr simulaciones y visualizar perfiles de deposición de energía en tiempo real
- **Pestaña de Explicaciones** (`InfoWidget`): documentación HTML renderizada de todas las fórmulas usadas en el pipeline de análisis — no más buscar en el código fuente qué significa r_rms

### Nuevo Formato de Exportación
- **Exportador de perfil de energía** (`EnergyProfileExporter`): exporta la curva de deposición de energía longitudinal (pico de Bragg) a CSV

### Correcciones de Bugs
- Inestabilidad numérica en `FocusDetector` cerca de configuraciones de haz degeneradas
- Casos extremos en `SliceExtractor` para pasos de tiempo muy dispersos
- Bug de acumulación en `FrameStatisticsEngine` cuando partículas se pierden antes del primer frame
- `LensDiskBuilder` generando mallas no-manifold en ciertas geometrías de envolvente
- `Geant4CsvImporter` fallando en archivos con conteo de columnas inconsistente
- `AxisFrameResolver` seleccionando el eje de propagación incorrecto en haces casi-diagonales
- `DatasetNormalizer` aplicando el factor de escala incorrecto con unidades de entrada ambiguas

---

## Características Principales

### 1. Importación Inteligente Multi-Formato
- **COMSOL CSV**: Detecta automáticamente formato "ancho" y "largo". Maneja encabezados con acentos y espacios raros.
- **Geant4 CSV**: Soporte completo para salida estándar de Geant4. Convierte automáticamente cm→m y ns→s.
- **CERN ROOT**: Soporte nativo en Linux (flag opcional). En Windows convertir a CSV primero.

### 2. Pipeline Completo de Análisis Físico
- Detección automática del sistema de ejes o sobrescritura manual (`--axis x/y/z`)
- Detección de foco: radio RMS, área de casco convexo, conteo de partículas
- Cálculo de envolvente del haz con casco convexo
- Reconstrucción de superficie caústica (paramétrica + malla)
- Generación de cuerpo de lente efectivo (lofting con espesor realista)
- Estadísticas frame por frame (activas, llegadas, perdidas, eficiencia, r_rms, sigma_x, sigma_z)

### 3. Simulador Biológico
- Pérdida de energía Bethe-Bloch a través de capas de tejido configurables
- Propagación de trayectorias de muones con straggling estocástico
- Planos de scoring para fluencia, espectro de energía y dosis
- Visualización del pico de Bragg y exportación a CSV

### 4. Interfaz Gráfica Profesional Qt6
- Tema oscuro optimizado para sesiones largas
- Visor 3D interactivo con VTK y slider de tiempo
- Dashboards en tiempo real (curva de foco, área de envolvente, conteo, distribución radial)
- Pestaña del simulador con gráficos de deposición de energía en vivo
- Pestaña de documentación matemática con todas las fórmulas renderizadas
- Exportación con un clic de todo (PDF, MP4, OBJ, CSV, PNG)

### 5. CLI Headless para Automatización
```bash
beamlab --input datos.csv --output resultados/corrida42 --caustic-points 128 --lens-layers 24
```
Perfecto para trabajos en cluster y procesamiento por lotes.

### 6. Reproducibilidad Bit-Exacta
Windows y Linux producen salida **bit-idéntica** (IEEE 754). Todo el pipeline de post-proceso está en C++20 puro con operaciones de punto flotante determinísticas y formato de salida `%.12e`.

### 7. Formatos de Exportación
| Formato | Contenido |
|---------|-----------|
| `.obj`  | Mallas 3D (resolución completa + vista previa decimada) |
| `.csv`  | Tablas, perfiles de energía, vista previa de trayectorias |
| `.png`  | Gráficos listos para publicación |
| `.mp4`  | Animación de evolución del haz (requiere ffmpeg) |
| `.pdf`  | Reporte multi-página con imágenes embebidas |
| `.txt`  | Ecuaciones de superficie paramétrica |
| `.json` | Metadata de la corrida y manifiesto |

---

## Instalación y Compilación

### Windows (Lo Más Fácil)

**Opción A — Instalador (Recomendado)**
1. Descargá `BeamLabStudio_Setup_Windows_x64.exe` desde Releases
2. Doble clic → Siguiente → Siguiente → Instalar (sin derechos de administrador)
3. La aplicación se abre automáticamente

**Opción B — Portable**
1. Descargá `BeamLabStudio_Portable_Windows_x64.exe`
2. Extraé donde quieras (funciona en pendrive)
3. Ejecutá `BeamLabStudio.exe`

> **Advertencia de OneDrive**: NO ejecutes el instalador desde una carpeta sincronizada con OneDrive. Mové el archivo a `C:\Downloads\` primero.

### Linux (Nativo — Recomendado para Desarrollo)

**Prerrequisitos**

Fedora/RHEL:
```bash
sudo dnf install cmake ninja gcc-c++ qt6-qtbase-devel qt6-qtcharts-devel \
                 vtk-devel nlohmann-json-devel
```

Ubuntu/Debian:
```bash
sudo apt install cmake ninja-build g++ qt6-base-dev qt6-charts-dev \
                 libvtk9-dev nlohmann-json3-dev
```

**Compilación**
```bash
git clone https://github.com/kegouro/BeamLabStudio.git
cd BeamLabStudio

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

./build/beamlab --help
./build/beamlab_ui
```

---

## Ejemplos de Uso

### CLI
```bash
# Análisis básico
beamlab datos/comsol_run.csv --output outputs/corrida_2026-05-07

# Opciones avanzadas
beamlab datos/geant4_muones.csv \
  --output resultados/terapia_muones_v3 \
  --axis z \
  --reference-mode axial-bins \
  --axial-bins 501 \
  --caustic-points 128 \
  --lens-layers 32
```

### GUI
```bash
beamlab_ui
# Archivo → Abrir → CSV → Analizar → Exportar todo
# O usá la pestaña Simulador para correr simulaciones de tejido Bethe-Bloch
```

---

## Arquitectura (Si el Repo Muere)

1. **Modelo de datos**: `Trajectory`, `TrajectorySample`, `TrajectoryDataset`, `AxisFrame`, `BeamEnvelope`, `LensSurfaceModel`, `ParticleInfo`
2. **Capa de importación**: `ComsolCsvImporter`, `Geant4CsvImporter`, `RootNativeImporter`
3. **Capa de análisis**: `FrameStatisticsEngine` → `FocusCurveBuilder` → `FocusDetector` → `SliceExtractor` → `ConvexHullEnvelopeExtractor` → `SurfaceBuilder` → `LensDiskBuilder`
4. **Capa de simulación**: `BetheBlochEngine` → `MuonTrackSimulator` → `TissueRegistry` / `TissueSlab` → `ScoringPlaneDetector`
5. **Capa UI**: `MainWindow`, `Scene3DWidget`, `StatsDashboardWidget`, `SimulatorWidget`, `InfoWidget`, `ObjViewerWidget`
6. **Capa de exportación**: `MeshExporter`, `AnalysisReportExporter`, `VisualizationDataExporter`, `ParametricSurfaceExporter`, `EnergyProfileExporter`

---

## Roadmap

- [x] Motor de análisis central (Linux + Windows bit-equivalente)
- [x] GUI Qt6 con visor 3D
- [x] Exportación PDF + MP4 + OBJ
- [x] Simulador de tejido biológico (Bethe-Bloch + tracking de muones)
- [x] Pestaña de simulador interactivo
- [x] Pestaña de documentación matemática con fórmulas renderizadas
- [x] Exportación de perfil de energía (pico de Bragg)
- [ ] GitHub Actions CI para releases automáticos (Linux + Windows cross-compile matrix)
- [ ] Firma de código para Windows (eliminar advertencia SmartScreen)
- [ ] AppImage para Linux
- [ ] Soporte CERN ROOT en Windows (bloqueado por complejidad del cross-compile)
- [ ] Tests de regresión bit-equivalencia Linux vs Windows en CI
- [ ] Straggling de Landau completo en el simulador (actualmente aproximación gaussiana)

---

## Licencia

MIT — **con cláusula extra**: el uso comercial o cualquier forma de monetización está prohibido sin permiso expreso por escrito de José Labarca. Aplica a BeamLabStudio y toda obra derivada, incluyendo MuonSimViewer.

Ver el archivo `LICENSE` en el repositorio.

---

## Créditos y Agradecimientos

- **Autor y Arquitecto**: José Labarca — Universidad Santa María / CCTVal (terapia con haces de muones contra el cáncer)
- **Librerías**: Qt 6, VTK, nlohmann/json, CERN ROOT, MinGW-w64, NSIS
- **Inspiración**: El grupo de muones del CCTVal que necesita buenas herramientas de análisis

---

**Si usás BeamLabStudio en tu investigación, por favor citame y mandame un mail.**  
Me encantaría saber que esta cosa que construí durante el infierno de exámenes está ayudando a ciencia real.

**Nos vemos en Karlsruhe, octubre 2026.**

— José

---

*Última actualización: mayo 2026 — BeamLabStudio v0.2.0*
