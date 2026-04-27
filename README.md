
                    BEAMLABSTUDIO v0.1.0 - README BILINGÜE 
          Plataforma Científica para Análisis de Haces y Reconstrucción Óptica
                            Por José Labarca
                          "Por y para el mundo"


================================================================================
                              ENGLISH VERSION
================================================================================

# BeamLabStudio v0.1.0

**Cross-platform scientific application for beam trajectory analysis, optical surface reconstruction, and Monte Carlo simulation post-processing.**

> **Author's Note (José Labarca):**  
> I study Physics (4th year, Universidad Santa María - Chile), not Computer Science.  
> I built this entire thing from scratch in roughly two weeks while preparing for exams,  
> taking my IELTS (C1), and surviving a Quantum Mechanics II test that went... let's just say "character building".  
> If you find a bug, please fix it for me — I'm probably at KIT in Germany by the time you read this.  
> The Linux build works perfectly. Windows works too (I cross-compiled it myself from Fedora).  
> Pull requests are extremely welcome. This project started as a quick viewer for muon beam simulations  
> at CCTVal for cancer therapy research... and then it exploded into something much bigger.

**This is the full professional rewrite of my earlier prototype [MuonSimViewer](https://github.com/kegouro/MuonSimViewer).**  
MuonSimViewer was the "weekend hack" version. BeamLabStudio is the production-grade, bit-equivalent,  
cross-platform, fully documented tool you can actually use in a real research lab.

---

## THE MANIFESTO (Please Read)

This software was created **for science, by a physicist who was tired of bad tools**.  
It exists **for the world** — for students, researchers, national labs, and anyone working on  
muon beam therapy for cancer at CCTVal-USM or anywhere else on Earth.

**However, with great power comes great responsibility:**

**COMMERCIAL USE, SALE, OR ANY FORM OF MONETIZATION IS STRICTLY PROHIBITED**  
without my explicit written permission. This includes BeamLabStudio itself and **all derivatives**,  
forks, or spiritual successors (including the original MuonSimViewer prototype).

This is academic and research software. I built it unpaid, during exam season, while learning COMSOL  
from zero in one week and then deciding the existing tools were not good enough.  
If you want to use it commercially, talk to me first. I'm not against making money —  
I'm against people making money off my all-nighters and quantum mechanics-induced stress.

---

## What is BeamLabStudio?

BeamLabStudio is a complete, self-contained scientific computing platform that takes raw  
particle trajectory data (from Geant4, COMSOL, or CERN ROOT) and turns it into:

- Beautiful 3D visualizations
- Quantitative beam physics metrics (focus position, RMS radius, envelope area, efficiency, etc.)
- Reconstructed optical surfaces (effective lens bodies, caustics)
- Publication-ready reports (PDF + CSV + OBJ meshes + parametric equations)
- Animated MP4 videos of the beam evolution

It was designed specifically for the muon beam cancer therapy project at CCTVal, but it is  
general enough for any charged particle beam transport problem.

---

## Key Features (Detailed)

### 1. Intelligent Multi-Format Import
- **COMSOL CSV**: Automatically detects both "wide" format (columns with @ t=...) and "long" format.  
  No manual column mapping required.
- **Geant4 CSV**: Full support for standard Geant4 output (x_cm, y_cm, z_cm, time_ns, trackID, eventID, etc.).  
  Automatically converts cm→m and ns→s.
- **CERN ROOT**: Native support on Linux (optional build flag). On Windows you must first convert to CSV.

### 2. Complete Physics Analysis Pipeline
- Automatic axis frame detection or manual override (--axis x/y/z)
- Focus detection using multiple metrics (RMS radius, envelope area, etc.)
- Beam envelope calculation with convex hull
- Caustic surface reconstruction (parametric + mesh)
- Effective lens body generation (lofted surfaces with realistic thickness)
- Frame-by-frame statistics (active particles, arrived, lost, efficiency, r_rms, sigma_x, sigma_z)

### 3. Professional Qt6 Graphical Interface
- Dark theme optimized for long analysis sessions
- Interactive 3D VTK viewer with time slider
- Real-time dashboards (focus curve, envelope area, point count, radial distribution)
- One-click export of everything (PDF report, MP4, OBJ, CSV, PNG plots)

### 4. Headless CLI for Automation
```bash
beamlab --input data.csv --output results/run42 --caustic-points 128 --lens-layers 24
```
Perfect for cluster jobs and batch processing.

### 5. Numerically Reproducible
The Windows and Linux versions produce **bit-identical** output (IEEE 754).  
This was achieved by rewriting the entire post-processing pipeline in pure C++.

### 6. Rich Export Formats
- 3D: .obj (full meshes + preview decimated versions)
- 2D: .png plots, .csv tables
- Video: .mp4 (requires ffmpeg)
- Reports: .pdf (multi-page with embedded images), .md summary, .json metadata
- Equations: parametric descriptions in .txt + sampled .csv

---

## Installation & Building

### Windows (Easiest)

**Option A - Installer (Recommended)**
1. Download `BeamLabStudio_Setup_Windows_x64.exe` from Releases
2. Double-click → Next → Next → Install (no admin rights needed)
3. The app launches automatically

**Option B - Portable**
1. Download `BeamLabStudio_Portable_Windows_x64.exe`
2. Extract anywhere (USB stick works)
3. Run `BeamLabStudio.exe`

> **OneDrive Warning**: Do NOT run the installer from a OneDrive-synced folder.  
> OneDrive sometimes leaves large .exe files as unsynced placeholders, causing "Installer integrity check failed".  
> Move the file to `C:\Downloads\` or `C:\Temp\` first.

### Linux (Native - Recommended for Development)

**Prerequisites**
```bash
sudo dnf install cmake ninja gcc-c++ qt6-qtbase-devel qt6-qtcharts-devel \
                 vtk-devel nlohmann-json-devel  # or equivalent on Ubuntu
```

**Build**
```bash
git clone https://github.com/kegouro/BeamLabStudio.git
cd BeamLabStudio

# Standard build (no ROOT)
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
See the full instructions in the original README — it involves MinGW-w64, NSIS, and careful DLL pruning.

---

## Usage Examples

### CLI (Headless)
```bash
# Basic analysis
beamlab data/comsol_run.csv --output outputs/run_2026-04-27

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
# Then: File → Open → select your CSV → Analyze → Export everything
```

---

## Output Directory Structure

Every analysis run creates a self-contained, publication-ready folder:

```
outputs/ui_run_<filename>_<timestamp>/
├── geometry/
│   ├── beam_caustic_surface.obj              # Full resolution caustic mesh
│   ├── beam_caustic_surface_preview.obj      # Decimated for fast viewing
│   ├── effective_lens_disk.obj
│   ├── effective_lens_body.obj               # The important one — 3D printable
│   └── trajectories_preview.obj              # All trajectories as polylines
├── visualization/
│   ├── visualization_manifest.json           # Index of every output file
│   ├── trajectories_preview.csv
│   ├── focal_slice_points.csv
│   └── envelope_rings.csv
├── plots/                                    # PNG figures (ready for papers)
├── reports/
│   ├── analysis_summary.md
│   ├── quality_report.md
│   ├── run_metadata.json
│   └── BeamLabStudio_Report.pdf              # Multi-page PDF with embedded images
├── equations/
│   ├── beam_caustic_parametric_equation.txt
│   └── effective_lens_body_parametric_equation.txt
├── tables/
│   ├── focus_curve.csv
│   └── envelope_summary.csv
└── logs/
```

---

## Technical Deep Dive (How It Actually Works)

### 1. The COMSOL Parser (The Hardest Part)
COMSOL can export trajectory data in two completely different formats:

- **Wide format**: One row per particle, hundreds of columns like `qx @ t=0.1ns`, `qy @ t=0.1ns`, ...
- **Long format**: One row per (particle, time) pair

BeamLabStudio automatically detects which format you have and parses it correctly.  
The parser even handles Spanish-accented headers (`posición de partícula`) and weird whitespace.

### 2. Focus Detection
We compute multiple metrics per time step:
- RMS beam radius
- Convex hull area
- Particle count inside acceptance aperture

The focus is defined as the time step with the global minimum of the chosen metric.  
You can override the metric with `--reference-mode`.

### 3. Surface Reconstruction
The "effective lens body" is generated by lofting the beam envelope at the focal slice  
and giving it realistic thickness (thicker in the center, thinner at the edges — like a real magnetic lens).

### 4. Bit-Exact Reproducibility
This was the biggest engineering challenge. The original Windows post-processing used PowerShell,  
which produced **geometrically different** lens bodies than the Python reference.

Solution: I rewrote every single line of the post-processing in pure C++20,  
using the exact same floating-point operations, iteration order, and output formatting (`%.12e`).  
Result: `diff` between Linux and Windows outputs shows zero differences.

---

## Building This From Scratch (If the Repo Dies)

If you ever need to rebuild BeamLabStudio from zero (God forbid), here is the minimal recipe:

1. **Core data model**: `Trajectory`, `TrajectorySample`, `TrajectoryDataset`, `AxisFrame`, `BeamEnvelope`, `LensSurfaceModel`
2. **Import layer**: One importer per format (ComsolCsvImporter, Geant4CsvImporter, RootNativeImporter)
3. **Analysis layer**: FrameStatisticsEngine → FocusCurveBuilder → FocusDetector → SliceExtractor → ConvexHullEnvelopeExtractor → SurfaceBuilder → LensDiskBuilder
4. **Visualization layer**: VtkSceneWidget (Qt + VTK), StatsDashboardWidget (QtCharts)
5. **Export layer**: MeshExporter (OBJ), PdfReportExporter, VideoExporter (ffmpeg), ParametricSurfaceExporter
6. **Post-processing**: The C++ port in `tools/postprocess_native/` must be kept bit-identical to the Python reference.

All the mathematical details (how we compute r_rms, how we loft the lens, how we detect focus) are documented in the source code comments.

---

## Roadmap (Honest)

- [x] Core analysis engine (Linux + Windows bit-equivalent)
- [x] Qt6 GUI with 3D viewer
- [x] PDF + MP4 + OBJ export
- [ ] GitHub Actions CI for automatic releases
- [ ] Code signing for Windows (kill the SmartScreen warning)
- [ ] AppImage for Linux
- [ ] Full CERN ROOT support in Windows build (currently Linux-only)
- [ ] Machine learning assisted focus detection (future research)
- [ ] WebAssembly version for browser demo

---

## License

MIT License — **with an important extra clause**:

**No commercial use or monetization of any kind is allowed** without explicit written permission from José Labarca.  
This applies to BeamLabStudio and every derivative work, including MuonSimViewer.

I built this for science and for the muon therapy project at CCTVal.  
Let's keep it that way.

Full license text is in the `LICENSE` file in the repository.

---

## Credits & Acknowledgments

- **Author & Architect**: José Labarca (Universidad Santa María, CCTVal — muon beam cancer therapy)
- **Windows packaging & C++ post-process port**: Claude (Anthropic) — thank you for the 3am debugging sessions
- **Libraries**: Qt 6, VTK, nlohmann/json, CERN ROOT, MinGW-w64, NSIS
- **Inspiration**: The muon group at CCTVal who actually need good analysis tools
- **Personal thanks**: To my professors who let me work on this instead of forcing me to use only COMSOL

---

**If you use BeamLabStudio in your research, please cite it and drop me an email.**  
I would love to know that this thing I built during exam hell is actually helping real science.

**See you at KIT, October 2026.**

— José

================================================================================
                           VERSIÓN EN ESPAÑOL
================================================================================

# BeamLabStudio v0.1.0

**Aplicación científica multiplataforma para análisis de trayectorias de haces, reconstrucción de superficies ópticas y post-proceso de simulaciones Monte Carlo.**

> **Nota del autor (José Labarca):**  
> Estudio Licenciatura en Física (4to año, Universidad Santa María - Chile), no Ingeniería Informática.  
> Hice todo esto desde cero en aproximadamente dos semanas mientras rendía certámenes,  
> daba el IELTS (saqué C1) y sobrevivía un examen de Cuántica II que fue... digamos que "formador de carácter".  
> Si encontrás un bug, por favor arreglalo por mí — probablemente ya estaré en KIT en Alemania cuando leas esto.  
> La versión Linux funciona perfecto. La de Windows también (la cross-compileé yo mismo desde Fedora).  
> Los pull requests son extremadamente bienvenidos. Este proyecto empezó como un visor rápido para simulaciones  
> de haces de muones en el CCTVal para investigación de terapia contra el cáncer... y después explotó en algo mucho más grande.

**Esta es la reescritura profesional completa de mi prototipo anterior [MuonSimViewer](https://github.com/kegouro/MuonSimViewer).**  
MuonSimViewer fue la versión "hack de fin de semana". BeamLabStudio es la herramienta de grado de producción,  
bit-equivalente, multiplataforma y completamente documentada que realmente podés usar en un laboratorio de investigación.

---

## EL MANIFIESTO (Por favor leé esto)

Este software fue creado **para la ciencia, por un físico harto de herramientas malas**.  
Existe **para el mundo** — para estudiantes, investigadores, laboratorios nacionales y cualquiera que esté trabajando  
en terapia con haces de muones para cáncer en el CCTVal-USM o en cualquier otro lugar de la Tierra.

**Sin embargo, con gran poder viene gran responsabilidad:**

**EL USO COMERCIAL, VENTA O CUALQUIER FORMA DE MONETIZACIÓN ESTÁ ESTRICTAMENTE PROHIBIDA**  
sin mi permiso expreso por escrito. Esto incluye BeamLabStudio mismo y **todos los derivados**,  
forks o sucesores espirituales (incluyendo el prototipo original MuonSimViewer).

Esto es software académico y de investigación. Lo construí sin cobrar, durante época de exámenes,  
mientras aprendía COMSOL desde cero en una semana y después decidí que las herramientas existentes no eran lo suficientemente buenas.  
Si querés usarlo comercialmente, hablame primero. No estoy en contra de hacer plata —  
estoy en contra de que alguien haga plata con mis noches en blanco y estrés por Cuántica II.

---

## ¿Qué es BeamLabStudio?

BeamLabStudio es una plataforma completa de computación científica autocontenida que toma datos crudos  
de trayectorias de partículas (de Geant4, COMSOL o CERN ROOT) y los convierte en:

- Visualizaciones 3D hermosas
- Métricas cuantitativas de física de haces (posición de foco, radio RMS, área de envolvente, eficiencia, etc.)
- Superficies ópticas reconstruidas (cuerpos de lente efectivos, caústicas)
- Reportes listos para publicación (PDF + CSV + mallas OBJ + ecuaciones paramétricas)
- Videos animados MP4 de la evolución del haz

Fue diseñado específicamente para el proyecto de terapia con haces de muones contra el cáncer en el CCTVal,  
pero es lo suficientemente general para cualquier problema de transporte de partículas cargadas.

---

## Características Principales (Detalladas)

### 1. Importación Inteligente Multi-Formato
- **COMSOL CSV**: Detecta automáticamente tanto el formato "ancho" (columnas con @ t=...) como el formato "largo".  
  No requiere mapeo manual de columnas.
- **Geant4 CSV**: Soporte completo para la salida estándar de Geant4 (x_cm, y_cm, z_cm, time_ns, trackID, eventID, etc.).  
  Convierte automáticamente cm→m y ns→s.
- **CERN ROOT**: Soporte nativo en Linux (flag de compilación opcional). En Windows primero tenés que convertir a CSV.

### 2. Pipeline Completo de Análisis Físico
- Detección automática del sistema de ejes o sobrescritura manual (--axis x/y/z)
- Detección de foco usando múltiples métricas (radio RMS, área de envolvente, etc.)
- Cálculo de envolvente del haz con casco convexo
- Reconstrucción de superficie caústica (paramétrica + malla)
- Generación de cuerpo de lente efectivo (superficies lofted con espesor realista)
- Estadísticas frame por frame (partículas activas, llegadas, perdidas, eficiencia, r_rms, sigma_x, sigma_z)

### 3. Interfaz Gráfica Profesional Qt6
- Tema oscuro optimizado para sesiones largas de análisis
- Visor 3D interactivo con VTK y slider de tiempo
- Dashboards en tiempo real (curva de foco, área de envolvente, conteo de puntos, distribución radial)
- Exportación con un clic de todo (reporte PDF, MP4, OBJ, CSV, gráficos PNG)

### 4. CLI Headless para Automatización
```bash
beamlab --input datos.csv --output resultados/corrida42 --caustic-points 128 --lens-layers 24
```
Perfecto para trabajos en cluster y procesamiento por lotes.

### 5. Reproducibilidad Numérica
Las versiones de Windows y Linux producen salida **bit-idéntica** (IEEE 754).  
Esto se logró reescribiendo todo el pipeline de post-proceso en C++ puro.

### 6. Formatos de Exportación Ricos
- 3D: .obj (mallas completas + versiones decimadas para vista previa)
- 2D: .png (gráficos), .csv (tablas)
- Video: .mp4 (requiere ffmpeg)
- Reportes: .pdf (multi-página con imágenes embebidas), .md resumen, .json metadata
- Ecuaciones: descripciones paramétricas en .txt + muestreo en .csv

---

## Instalación y Compilación

### Windows (Lo Más Fácil)

**Opción A - Instalador (Recomendado)**
1. Descargá `BeamLabStudio_Setup_Windows_x64.exe` desde Releases
2. Doble clic → Siguiente → Siguiente → Instalar (no requiere derechos de administrador)
3. La aplicación se abre automáticamente

**Opción B - Portable**
1. Descargá `BeamLabStudio_Portable_Windows_x64.exe`
2. Extraé donde quieras (funciona en pendrive)
3. Ejecutá `BeamLabStudio.exe`

> **Advertencia de OneDrive**: NO ejecutes el instalador desde una carpeta sincronizada con OneDrive.  
> OneDrive a veces deja archivos .exe grandes como placeholders sin sincronizar, causando "Installer integrity check failed".  
> Mové el archivo primero a `C:\Downloads\` o `C:\Temp\`.

### Linux (Nativo - Recomendado para Desarrollo)

**Prerrequisitos**
```bash
sudo apt install cmake ninja-build g++ qt6-base-dev qt6-charts-dev \
                 libvtk9-dev nlohmann-json3-dev  # o equivalente en tu distro
```

**Compilación**
```bash
git clone https://github.com/kegouro/BeamLabStudio.git
cd BeamLabStudio

# Compilación estándar (sin ROOT)
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# Con soporte para CERN ROOT (opcional)
cmake -S . -B build-root -G Ninja -DBEAMLAB_ENABLE_ROOT=ON
cmake --build build-root -j$(nproc)

# Ejecutar
./build/beamlab --help
./build/beamlab_ui
```

**Cross-compilación de binarios Windows desde Fedora (para mantenedores)**
Ver las instrucciones completas en el README original — involucra MinGW-w64, NSIS y podado cuidadoso de DLLs.

---

## Ejemplos de Uso

### CLI (Sin interfaz)
```bash
# Análisis básico
beamlab datos/comsol_run.csv --output outputs/corrida_2026-04-27

# Opciones avanzadas
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

### GUI
```bash
beamlab_ui
# Luego: Archivo → Abrir → seleccioná tu CSV → Analizar → Exportar todo
```

---

## Estructura del Directorio de Salida

Cada corrida de análisis crea una carpeta autocontenida lista para publicación:

```
outputs/ui_run_<archivo>_<timestamp>/
├── geometry/
│   ├── beam_caustic_surface.obj              # Malla caústica de resolución completa
│   ├── beam_caustic_surface_preview.obj      # Decimada para vista rápida
│   ├── effective_lens_disk.obj
│   ├── effective_lens_body.obj               # El importante — imprimible en 3D
│   └── trajectories_preview.obj              # Todas las trayectorias como polilíneas
├── visualization/
│   ├── visualization_manifest.json           # Índice de todos los archivos de salida
│   ├── trajectories_preview.csv
│   ├── focal_slice_points.csv
│   └── envelope_rings.csv
├── plots/                                    # Figuras PNG (listas para papers)
├── reports/
│   ├── analysis_summary.md
│   ├── quality_report.md
│   ├── run_metadata.json
│   └── BeamLabStudio_Report.pdf              # PDF multi-página con imágenes embebidas
├── equations/
│   ├── beam_caustic_parametric_equation.txt
│   └── effective_lens_body_parametric_equation.txt
├── tables/
│   ├── focus_curve.csv
│   └── envelope_summary.csv
└── logs/
```

---

## Profundización Técnica (Cómo Funciona Realmente)

### 1. El Parser de COMSOL (La Parte Más Difícil)
COMSOL puede exportar datos de trayectorias en dos formatos completamente diferentes:

- **Formato ancho**: Una fila por partícula, cientos de columnas como `qx @ t=0.1ns`, `qy @ t=0.1ns`, ...
- **Formato largo**: Una fila por cada par (partícula, tiempo)

BeamLabStudio detecta automáticamente qué formato tenés y lo parsea correctamente.  
El parser incluso maneja encabezados con acentos en español (`posición de partícula`) y espacios raros.

### 2. Detección de Foco
Calculamos múltiples métricas por paso de tiempo:
- Radio RMS del haz
- Área del casco convexo
- Cantidad de partículas dentro de la apertura de aceptación

El foco se define como el paso de tiempo con el mínimo global de la métrica elegida.  
Podés sobrescribir la métrica con `--reference-mode`.

### 3. Reconstrucción de Superficies
El "cuerpo de lente efectivo" se genera haciendo lofting de la envolvente del haz en el slice focal  
y dándole espesor realista (más grueso en el centro, más delgado en los bordes — como una lente magnética real).

### 4. Reproducibilidad Bit-Exacta
Este fue el desafío de ingeniería más grande. El post-proceso original de Windows usaba PowerShell,  
que producía **cuerpos de lente geométricamente diferentes** que la referencia en Python.

Solución: Reescribí cada línea del post-proceso en C++20 puro,  
usando exactamente las mismas operaciones de punto flotante, orden de iteración y formato de salida (`%.12e`).  
Resultado: `diff` entre salidas de Linux y Windows muestra cero diferencias.

---

## Cómo Reconstruir Esto Desde Cero (Si el Repo Muere)

Si alguna vez necesitás reconstruir BeamLabStudio desde cero (que Dios no lo quiera), acá está la receta mínima:

1. **Modelo de datos central**: `Trajectory`, `TrajectorySample`, `TrajectoryDataset`, `AxisFrame`, `BeamEnvelope`, `LensSurfaceModel`
2. **Capa de importación**: Un importer por formato (ComsolCsvImporter, Geant4CsvImporter, RootNativeImporter)
3. **Capa de análisis**: FrameStatisticsEngine → FocusCurveBuilder → FocusDetector → SliceExtractor → ConvexHullEnvelopeExtractor → SurfaceBuilder → LensDiskBuilder
4. **Capa de visualización**: VtkSceneWidget (Qt + VTK), StatsDashboardWidget (QtCharts)
5. **Capa de exportación**: MeshExporter (OBJ), PdfReportExporter, VideoExporter (ffmpeg), ParametricSurfaceExporter
6. **Post-proceso**: El puerto en C++ de `tools/postprocess_native/` debe mantenerse bit-idéntico a la referencia Python.

Todos los detalles matemáticos (cómo calculamos r_rms, cómo hacemos lofting del lente, cómo detectamos el foco) están documentados en los comentarios del código fuente.

---

## Roadmap (Honesto)

- [x] Motor de análisis central (Linux + Windows bit-equivalente)
- [x] GUI Qt6 con visor 3D
- [x] Exportación PDF + MP4 + OBJ
- [ ] GitHub Actions CI para releases automáticos
- [ ] Firma de código para Windows (matar el warning de SmartScreen)
- [ ] AppImage para Linux
- [ ] Soporte completo de CERN ROOT en el build de Windows (actualmente solo Linux)
- [ ] Detección de foco asistida por machine learning (investigación futura)
- [ ] Versión WebAssembly para demo en navegador

---

## Licencia

Licencia MIT — **con una cláusula extra importante**:

**El uso comercial o cualquier forma de monetización está prohibido** sin permiso expreso por escrito de José Labarca.  
Esto aplica a BeamLabStudio y a toda obra derivada, incluyendo MuonSimViewer.

Lo construí para la ciencia y para el proyecto de terapia con muones del CCTVal.  
Mantengámoslo así.

El texto completo de la licencia está en el archivo `LICENSE` del repositorio.

---

## Créditos y Agradecimientos

- **Autor y Arquitecto**: José Labarca (Universidad Santa María, CCTVal — terapia con haces de muones contra el cáncer)
- **Empaquetado Windows y puerto C++ del post-proceso**: Claude (Anthropic) — gracias por las sesiones de debugging a las 3am
- **Librerías**: Qt 6, VTK, nlohmann/json, CERN ROOT, MinGW-w64, NSIS
- **Inspiración**: El grupo de muones del CCTVal que realmente necesita buenas herramientas de análisis
- **Agradecimientos personales**: A mis profesores que me dejaron trabajar en esto en vez de obligarme a usar solo COMSOL

---

**Si usás BeamLabStudio en tu investigación, por favor citame y mandame un mail.**  
Me encantaría saber que esta cosa que construí durante el infierno de exámenes está ayudando a ciencia real.

**Nos vemos en Karlsruhe, octubre 2026.**

— José

================================================================================
                              FIN DEL ARCHIVO
================================================================================

Este README fue hechi el 27 de abril de 2026 a las 00:19 (hora de Chile).
Versión: 2.0 
```
