# BeamLabStudio

> **Nota del autor (José):** hice esto sin tener idea de que estoy haciendo. Si encuentras un error, soluciónalo por mí jajaja. La distro de Linux funciona sí o sí, la de Windows debería funcionar, pero si algo se rompe — pull requests welcome.

Plataforma científica modular para análisis de haces, trayectorias y geometría. Importa CSV de Geant4 / COMSOL (y opcionalmente archivos `.root` de CERN ROOT en builds Linux con `-DBEAMLAB_ENABLE_ROOT=ON`), corre el motor de análisis, y genera dashboards, mallas OBJ, gráficos PNG, vídeos MP4 y reportes PDF.

---

## Descargas

### Windows (x86_64)

| Archivo | Tamaño | Cuándo usarlo |
| --- | --: | --- |
| **`BeamLabStudio_Setup_Windows_x64.exe`** | 41 MB | Instalador per-user. Se instala en `%LOCALAPPDATA%\BeamLabStudio`, crea acceso directo en escritorio, registra uninstaller. **No requiere admin.** Recomendado para uso normal. |
| **`BeamLabStudio_Portable_Windows_x64.exe`** | 41 MB | Versión portable. Extrae a la carpeta que elijas y arranca. No toca el registro. Copiable a USB. |

> ⚠️ **Importante**: ejecuta el `.exe` desde una carpeta local (`C:\Users\TuUsuario\Downloads\`, `C:\Temp\`, etc.). **No lo corras directamente desde una carpeta que esté dentro de OneDrive** — OneDrive a veces deja archivos `.exe` grandes como placeholders sin sincronizar y eso produce el error _"Installer integrity check has failed"_. Si ves ese error, mueve el `.exe` fuera de OneDrive y vuelve a intentarlo.

### Linux

La distribución Linux es la canónica y se compila desde el código fuente con CMake + Ninja. Ver [Build desde código fuente](#build-desde-código-fuente).

---

## Cómo usar (Windows)

1. Doble clic en el `.exe`. El instalador es de 3 clics: _Siguiente → Siguiente → Instalar_.
2. La aplicación arranca sola al terminar.
3. Botón **Open + Analyze**, eliges tu CSV / `.root`, y la app corre el pipeline completo.
4. Los resultados quedan en `<carpeta_de_instalación>\outputs\ui_run_<archivo>_<timestamp>\`.

---

## Cómo usar (Linux)

```bash
# Compilar el motor
cmake -S . -B build -G Ninja
cmake --build build

# (opcional) Compilar con soporte nativo CERN ROOT
cmake -S . -B build-root -G Ninja -DBEAMLAB_ENABLE_ROOT=ON
cmake --build build-root

# (opcional) Compilar el UI Qt
cmake -S . -B build-ui -G Ninja -DBEAMLAB_ENABLE_QT_UI=ON
cmake --build build-ui

# Correr una análisis
scripts/run_beamlab_full.sh /ruta/a/datos.csv outputs/run_demo \
  --window 5 --caustic-points 96 --lens-points 128 --lens-layers 16

# o con UI:
build-ui/beamlab_ui
```

---

## Cómo se construye la entrega Windows desde Fedora

Esto es para que cualquiera (incluyéndome a mí mismo en seis meses) pueda regenerar los `.exe` desde cero.

### Dependencias en Fedora

```bash
sudo dnf install \
  mingw64-gcc-c++ mingw64-qt6-qtbase mingw64-qt6-qtsvg \
  mingw64-nlohmann-json \
  cmake ninja zip rsync \
  mingw32-nsis        # o instalar makensis manualmente
```

### Pasos

```bash
# 1. Compilar beamlab.exe + beamlab_ui.exe (motor + UI Qt) cross-compilados
bash scripts/release/build_windows_portable_from_fedora.sh
# Genera dist/BeamLabStudio-win64-portable/

# 2. Compilar beamlab_postprocess.exe (port C++ del postprocess Python)
x86_64-w64-mingw32-g++ -O2 -static -std=c++17 \
  tools/postprocess_native/beamlab_postprocess.cpp \
  -o dist/BeamLabStudio-win64-portable/beamlab_postprocess.exe

# 3. Compilar el launcher silencioso
x86_64-w64-mingw32-g++ -O2 -static -mwindows \
  tools/postprocess_native/BeamLabStudio_Launcher.cpp \
  -o dist/BeamLabStudio-win64-portable/BeamLabStudio.exe

# 4. Reemplazar el runner y eliminar el postprocess PowerShell incorrecto
cp tools/postprocess_native/run_beamlab_full.cmd \
   dist/BeamLabStudio-win64-portable/scripts/run_beamlab_full.cmd
rm -f dist/BeamLabStudio-win64-portable/scripts/windows/postprocess_visualization.ps1

# 5. Podar Qt6 DLLs no usadas (cierre transitivo de dependencias) — ahorra ~100 MB
bash tools/postprocess_native/prune_dist.sh

# 6. Empaquetar en single-exe con NSIS
makensis tools/postprocess_native/BeamLabStudio_installer.nsi
makensis tools/postprocess_native/BeamLabStudio_portable.nsi
```

---

## Decisiones técnicas que hay que conocer

(Esta sección es para ti, futuro-yo, o para quien tenga que mantener esto.)

### El motor (`beamlab.exe`) es bit-equivalente entre Linux y Windows

- Mismo `src/`, mismo C++ 20, mismo `-O2` sin `-ffast-math` ni `-funsafe-math-optimizations`.
- Cross-compilado con `x86_64-w64-mingw32-g++` desde Fedora. Aritmética IEEE 754 estándar.
- Resultados numéricos del motor son idénticos a nivel de bits.

### El postprocess fue reescrito en C++ nativo (importante)

La versión anterior del paquete Windows usaba un script PowerShell (`postprocess_visualization.ps1`) en lugar de los scripts Python originales (`tools/converters/lens_disk_to_body.py`, `tools/converters/trajectory_csv_to_obj.py`). **Las fórmulas del PowerShell no eran equivalentes:**

| Concepto | Python original | PowerShell que reemplazaba |
| --- | --- | --- |
| Apertura del lens body | `max(radial_distances)` | `percentile(r, 0.98) · 1.15` |
| Half-thickness | `½·[t_edge + (t_center − t_edge)·(1 − ρ²)^p]`, `p=1.85` | `t · (1 − ρ²)` (efectivamente `p=1`, sin `t_edge`) |
| `t_center` / `t_edge` | `0.35·R` y `0.06·R` | `max(0.35·R, 0.001)`, no distingue |
| Caras laterales del lens body | Triangulación correcta vía `boundary_edges()` | No genera caras laterales |
| `effective_lens_body.obj` | Sí | No (solo modificaba el disk) |
| `parametric_equation.txt`, `metadata.json` | Sí | No |

→ Geometrías de salida distintas, lo cual afecta cualquier análisis óptico downstream.

**Lo que hice:** reescribí los cuatro scripts Python en C++ en un solo binario (`beamlab_postprocess.exe`), replicando bit-a-bit:

- Las mismas operaciones IEEE 754 (`std::sqrt`, `std::pow`, `+`, `-`, `*`, `/`).
- El mismo orden de operaciones.
- El mismo formato de salida (`%.12e`).
- El mismo orden de inserción de claves en `defaultdict` (replicado con `vector<key>` auxiliar + `unordered_map`).
- Detección de `boundary_edges` con el mismo criterio (`count == 1`) y el mismo orden de aparición.
- JSON con `indent=2` y orden de claves preservado (`nlohmann::ordered_json`).

**Validación:** ejecuté el postprocess Python original y el C++ sobre el mismo fixture sintético. Resultado del diff (con paths absolutos neutralizados):

```
✓ IDÉNTICO: geometry/effective_lens_body.obj
✓ IDÉNTICO: geometry/effective_lens_body_preview.obj
✓ IDÉNTICO: geometry/trajectories_preview.obj
✓ IDÉNTICO: equations/effective_lens_body_parametric_equation.txt
✓ IDÉNTICO: visualization/visualization_manifest.json
✓ IDÉNTICO: reports/effective_lens_body_metadata.json
```

`tools/postprocess_native/validate.sh` re-corre esa validación si quieres auditarla.

### El runner pasa todos los argumentos del UI al motor

El `run_beamlab_full.cmd` original hardcodeaba `--window 5 --caustic-points 96 --lens-points 128 --lens-layers 16` y descartaba lo que el UI calculaba en `defaultAnalysisArguments()` (`--axis z`, `--reference-mode axial-bins`, `--axial-bins 501`, `--preview-trajectories 10000`, etc.). El nuevo `.cmd` recoge todos los args con `shift` + bucle y los pasa intactos.

### Bug del `!` en paths (resuelto)

⚠️ Si estás escribiendo un `.cmd` para Windows que va a recibir paths arbitrarios, **no uses `setlocal EnableDelayedExpansion`**. Con delayed expansion activa, los `!` dentro de variables tienen significado especial (`!variable!`) y cualquier path que contenga `!` se rompe — el `!` desaparece silenciosamente. Esto pasó en una versión anterior del runner: una carpeta llamada `BEAMLABSTUDIO WINDOWS IS ALIVE!` causaba que el runner buscara `beamlab.exe` en `BEAMLABSTUDIO WINDOWS IS ALIVE\` (sin `!`), y todo fallaba. La solución es no activar delayed expansion y usar concatenación normal con `set "VAR=%VAR% %1"`.

### DLLs Qt podadas

El dist anterior arrastraba ~155 DLLs (Qt6Quick\*, Qt6Designer\*, Qt6Help, Qt6Sql, Qt6Charts, Qt6QmlCompiler...) que `beamlab_ui.exe` ni siquiera enlaza. `prune_dist.sh` hace cierre transitivo desde los `.exe` y los plugins Qt cargados dinámicamente, elimina las 97 DLLs sobrantes (~97 MB), y verifica que cada DLL del import table queda presente. Resultado: dist de 110 MB en vez de 205 MB, comprimido a 41 MB.

Las dependencias finales son:

```
BeamLabStudio.exe       → KERNEL32, USER32, msvcrt          (todo Win API)
beamlab.exe             → KERNEL32, msvcrt, libstdc++-6, libgcc_s_seh-1
beamlab_postprocess.exe → KERNEL32, msvcrt                  (estático)
beamlab_ui.exe          → ... + Qt6Core, Qt6Gui, Qt6Widgets
                            → ICU 76, freetype, harfbuzz, png/jpeg, pcre2, glib, etc.
```

Más los plugins Qt: `platforms/qwindows.dll`, `styles/qmodernwindowsstyle.dll`, `imageformats/{qsvg,qico,qjpeg,qgif}.dll`, `iconengines/qsvgicon.dll`.

---

## Estructura del repo

```
BeamLabStudio/
├── src/
│   ├── app/                         ← bootstrap, main del motor CLI
│   ├── analysis/                    ← focus, slice, envelope, surfaces, statistics
│   ├── core/, data/, geometry/      ← tipos y kernels
│   ├── io/                          ← importers (CSV/COMSOL/Geant4/ROOT) y exporters
│   ├── render/                      ← exporters de visualización
│   └── ui/qt/                       ← UI Qt (MainWindow, dashboards, viewer 3D)
├── tools/
│   ├── converters/                  ← scripts Python originales (referencia)
│   ├── postprocess/                 ← wrappers Python originales (referencia)
│   └── postprocess_native/          ← reescritura C++ nativa para Windows
│       ├── beamlab_postprocess.cpp  ← port bit-equivalente de los scripts Python
│       ├── BeamLabStudio_Launcher.cpp ← launcher Windows sin ventana CMD
│       ├── run_beamlab_full.cmd     ← runner Windows (sin delayed expansion)
│       ├── BeamLabStudio_installer.nsi
│       ├── BeamLabStudio_portable.nsi
│       ├── prune_dist.sh            ← cierre transitivo de DLLs
│       └── validate.sh              ← diff Python vs C++ del postprocess
├── scripts/
│   ├── run_beamlab_full.sh          ← runner Linux (canónico)
│   ├── run_beamlab_full.py          ← orquestador Python (Linux)
│   └── release/
│       └── build_windows_portable_from_fedora.sh
├── tests/, examples/, docs/, cmake/
└── CMakeLists.txt
```

---

## Roadmap / TODO honesto

- [ ] Generar releases de GitHub Actions automáticamente al taggear (matrix Linux + Windows-cross-from-Fedora).
- [ ] Firmar los `.exe` (signtool / sigstore) para evitar el SmartScreen warning de Windows en máquinas nuevas.
- [ ] AppImage para Linux para tener distribución "doble clic" también ahí.
- [ ] Soporte nativo CERN ROOT en el bundle Windows (hoy está deshabilitado en cross-build; los .root se procesan solo en Linux).
- [ ] Validar bit-equivalencia del postprocess C++ contra una corrida real grande (no solo el fixture sintético).
- [ ] Tests automatizados de regresión que comparen outputs Linux vs Windows en CI.

---

## Licencia
MIT License

Copyright (c) 2026 José Labarca

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
EOF

---

## Créditos

- Autor: José Labarca
- Asistencia con el empaquetado Windows / port C++ del postprocess: Claude (Anthropic)
- Qt 6 (LGPL), nlohmann/json (MIT), MinGW-w64 (GPL/proprietary mix), NSIS (zlib license)
