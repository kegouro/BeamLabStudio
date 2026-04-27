# BeamLabStudio

> **Note from the author (José):** I built this without really knowing what I was doing —
> I study Physics, not Computer Science. If you find a bug, fix it for me lol.
> The Linux build works for sure. Windows should work too, but if something breaks,
> pull requests are very welcome. I'll be at KIT in October so response times may vary 👀

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
![C++20](https://img.shields.io/badge/C%2B%2B-20-blue)
![Qt6](https://img.shields.io/badge/Qt-6-brightgreen)
![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20Windows%20x64-lightgrey)
![Status](https://img.shields.io/badge/status-v0.1.0%20stable-success)

**BeamLabStudio** is a cross-platform scientific application for beam trajectory analysis,
optical surface reconstruction, and Monte Carlo simulation postprocessing.

It was built from scratch in ~2 weeks (with exams in between, a C1 IELTS, and a quantum
mechanics test that went poorly — don't ask) because COMSOL was uncomfortable for batch
analysis and writing a Python script for every single run was a nightmare.
So I built this instead. It got a little out of hand.

The predecessor was [MuonSimViewer](https://github.com/kegouro/MuonSimViewer),
a prototype built in ~1 week. This is the full rewrite.

---

## Features

- **Multi-format import**: Geant4 CSV, COMSOL CSV, CERN ROOT (Linux only — see below)
- **Full analysis pipeline**: trajectories, focal analysis, beam envelope, optical surfaces, statistics
- **Qt6 GUI** with tabbed dashboards, 3D viewer, and real-time analysis log
- **Headless CLI** for batch processing and scripting
- **Export everything**: OBJ (3D geometry), PNG (2D plots), MP4 (animations), PDF (reports), CSV
- **Cross-platform**: Linux native + Windows portable single `.exe`
- **Numerically validated**: Windows and Linux outputs are bit-for-bit identical (IEEE 754)

---

## Downloads

### Windows (x86_64)

| File | Size | When to use |
| --- | --: | --- |
| **`BeamLabStudio_Setup_Windows_x64.exe`** | 41 MB | Standard installer. Installs to `%LOCALAPPDATA%\BeamLabStudio`, creates desktop shortcut, registers uninstaller. **No admin required.** |
| **`BeamLabStudio_Portable_Windows_x64.exe`** | 41 MB | Portable version. Extract anywhere, run immediately. No registry changes. USB-friendly. |

See [Releases](../../releases) for downloads.

> ⚠️ **OneDrive users**: do not run the installer directly from a OneDrive-synced folder.
> OneDrive sometimes leaves large `.exe` files as unsynced placeholders, which causes
> _"Installer integrity check has failed"_. Just move the file to `Downloads\` or `C:\Temp\`
> and run it from there.

### Supported input formats

| Format | Linux | Windows |
| --- | :---: | :---: |
| Geant4 CSV | ✅ | ✅ |
| COMSOL CSV | ✅ | ✅ |
| CERN ROOT (`.root`) | ✅ | ❌ |

> ROOT support on Windows would require cross-compiling the entire CERN ROOT framework
> for MinGW64, which is its own circle of hell. For now: convert `.root` to CSV on Linux
> using `tools/converters/root_to_beamlab_csv.py`, then use the CSV on Windows.

---

## How to use (Windows)

1. Download and double-click the `.exe`. Three clicks: _Next → Next → Install_.
2. The app launches automatically when done.
3. Click **Open + Analyze**, select your CSV or `.root` file, and the full pipeline runs.
4. Results are saved to `<install_dir>\outputs\ui_run_<filename>_<timestamp>\`.

---

## How to use (Linux)

```bash
# Build the analysis engine
cmake -S . -B build -G Ninja
cmake --build build

# Optional: build with CERN ROOT support
cmake -S . -B build-root -G Ninja -DBEAMLAB_ENABLE_ROOT=ON
cmake --build build-root

# Optional: build the Qt6 GUI
cmake -S . -B build-ui -G Ninja -DBEAMLAB_ENABLE_QT_UI=ON
cmake --build build-ui

# Run analysis from CLI
scripts/run_beamlab_full.sh /path/to/data.csv outputs/run_demo \
  --window 5 --caustic-points 96 --lens-points 128 --lens-layers 16

# Or launch the GUI
build-ui/beamlab_ui
```

---

## Output structure

Every analysis run produces a self-contained output directory:
outputs/ui_run_<file>_<timestamp>/
├── geometry/
│   ├── effective_lens_disk.obj          ← raw aperture mesh
│   ├── effective_lens_body.obj          ← reconstructed 3D lens body
│   ├── effective_lens_body_preview.obj
│   └── trajectories_preview.obj
├── visualization/
│   ├── visualization_manifest.json      ← index of all output files
│   └── trajectories_preview.csv
├── plots/                               ← PNG exports
├── reports/                             ← PDF report + metadata JSON
├── equations/                           ← parametric equations (TXT)
└── logs/

---

## Repository structure
BeamLabStudio/
├── src/
│   ├── app/            ← CLI entry point
│   ├── analysis/       ← focus, slice, envelope, surfaces, statistics
│   ├── core/           ← types, math kernels
│   ├── data/           ← data models
│   ├── geometry/       ← 3D geometry processing
│   ├── io/             ← importers (Geant4, COMSOL, ROOT) and exporters
│   ├── render/         ← visualization exporters
│   └── ui/qt/          ← Qt6 GUI (MainWindow, dashboards, 3D viewer)
├── tools/
│   ├── converters/     ← Python reference scripts
│   ├── postprocess/    ← Python postprocess wrappers (reference)
│   └── postprocess_native/
│       ├── beamlab_postprocess.cpp      ← C++ port (bit-equivalent to Python)
│       ├── BeamLabStudio_Launcher.cpp   ← silent Windows launcher
│       ├── run_beamlab_full.cmd         ← Windows runner (fixed)
│       ├── BeamLabStudio_installer.nsi  ← NSIS installer script
│       ├── BeamLabStudio_portable.nsi   ← NSIS portable script
│       ├── prune_dist.sh                ← DLL dependency closure
│       └── validate.sh                  ← bit-equivalence test
├── scripts/
│   ├── run_beamlab_full.sh              ← Linux runner (canonical)
│   ├── run_beamlab_full.py              ← Python orchestrator
│   └── release/
│       └── build_windows_portable_from_fedora.sh
├── tests/, examples/, docs/, cmake/
└── CMakeLists.txt

---

## Building the Windows distribution from Fedora

This section exists so future-me (or anyone else) can reproduce the Windows build
from scratch without having to remember anything.

### Dependencies

```bash
sudo dnf install \
  mingw64-gcc-c++ mingw64-qt6-qtbase mingw64-qt6-qtsvg \
  mingw64-nlohmann-json cmake ninja zip rsync mingw32-nsis
```

### Steps

```bash
# 1. Cross-compile beamlab.exe + beamlab_ui.exe
bash scripts/release/build_windows_portable_from_fedora.sh

# 2. Build the native postprocess binary
x86_64-w64-mingw32-g++ -O2 -static -std=c++17 \
  tools/postprocess_native/beamlab_postprocess.cpp \
  -o dist/BeamLabStudio-win64-portable/beamlab_postprocess.exe

# 3. Build the silent launcher
x86_64-w64-mingw32-g++ -O2 -static -mwindows \
  tools/postprocess_native/BeamLabStudio_Launcher.cpp \
  -o dist/BeamLabStudio-win64-portable/BeamLabStudio.exe

# 4. Replace the runner, remove the broken PowerShell postprocess
cp tools/postprocess_native/run_beamlab_full.cmd \
   dist/BeamLabStudio-win64-portable/scripts/run_beamlab_full.cmd
rm -f dist/BeamLabStudio-win64-portable/scripts/windows/postprocess_visualization.ps1

# 5. Prune unused Qt6 DLLs (transitive dependency closure, saves ~100 MB)
bash tools/postprocess_native/prune_dist.sh

# 6. Package into single-exe with NSIS
makensis tools/postprocess_native/BeamLabStudio_installer.nsi
makensis tools/postprocess_native/BeamLabStudio_portable.nsi
```

---

## Technical notes

### Bit-equivalent numerics across platforms

The Windows `beamlab.exe` is cross-compiled from the exact same `src/` as the Linux build,
using `x86_64-w64-mingw32-g++` with `-O2` and no unsafe math flags (`-ffast-math`,
`-funsafe-math-optimizations`). IEEE 754 double-precision arithmetic is identical on both
platforms — same source, same compiler family, same results.

### Why the postprocess was rewritten in C++

The previous Windows distribution used a PowerShell script
(`postprocess_visualization.ps1`) instead of the original Python tools.
The formulas were not equivalent:

| | Python (original) | PowerShell (replaced) |
| --- | --- | --- |
| Aperture radius | `max(radial_distances)` | `percentile(r, 0.98) × 1.15` |
| Half-thickness | `½·[t_edge + (t_center − t_edge)·(1−ρ²)^p]`, `p=1.85` | `t·(1−ρ²)` — effectively `p=1`, no `t_edge` |
| `t_center` / `t_edge` | `0.35·R` and `0.06·R` separately | `max(0.35·R, 0.001)`, not distinguished |
| Lateral faces | Correct triangulation via `boundary_edges()` | Not generated (open mesh) |
| `effective_lens_body.obj` | ✅ | ❌ |
| `parametric_equation.txt`, `metadata.json` | ✅ | ❌ |

This produced geometrically different lens bodies, which affects any downstream
optical analysis. The fix was to port all four Python scripts to a single C++ binary
(`beamlab_postprocess.exe`), replicating the same IEEE 754 operations, same iteration
order, same output format (`%.12e`), and same JSON key ordering (`nlohmann::ordered_json`).

Validation result (bit-level diff, absolute paths neutralized):
✓ IDENTICAL: geometry/effective_lens_body.obj
✓ IDENTICAL: geometry/effective_lens_body_preview.obj
✓ IDENTICAL: geometry/trajectories_preview.obj
✓ IDENTICAL: equations/effective_lens_body_parametric_equation.txt
✓ IDENTICAL: visualization/visualization_manifest.json
✓ IDENTICAL: reports/effective_lens_body_metadata.json

Run `tools/postprocess_native/validate.sh` to re-run the validation yourself.

### The `!` path bug (fixed)

If you're writing a `.cmd` script that receives arbitrary paths: **do not use
`setlocal EnableDelayedExpansion`**. With delayed expansion active, `!` characters
inside variables are treated as delimiters and silently consumed. A folder named
`BEAMLABSTUDIO WINDOWS IS ALIVE!` caused the runner to look for `beamlab.exe` in
`BEAMLABSTUDIO WINDOWS IS ALIVE\` — without the `!` — and fail silently.
Fixed by removing delayed expansion and using standard `set "VAR=%VAR% %1"` concatenation.

### DLL pruning

The original dist shipped ~155 DLLs including Qt6Quick\*, Qt6Designer\*, Qt6Help,
Qt6Sql, Qt6Charts, Qt6QmlCompiler, and others that `beamlab_ui.exe` never loads.
`prune_dist.sh` computes the transitive closure of imports from all `.exe` and Qt
plugin entry points, removes 97 unused DLLs (~97 MB), and verifies every remaining
import is satisfied. Final dist: 110 MB uncompressed, 41 MB as NSIS installer.

Final runtime dependencies:
BeamLabStudio.exe       → KERNEL32, USER32, msvcrt
beamlab.exe             → KERNEL32, msvcrt, libstdc++-6, libgcc_s_seh-1
beamlab_postprocess.exe → KERNEL32, msvcrt  (fully static)
beamlab_ui.exe          → Qt6Core, Qt6Gui, Qt6Widgets
ICU 76, freetype, harfbuzz, libpng, libjpeg,
pcre2, glib, iconv, zlib

Qt plugins: `platforms/qwindows.dll`, `styles/qmodernwindowsstyle.dll`,
`imageformats/{qsvg,qico,qjpeg,qgif}.dll`, `iconengines/qsvgicon.dll`.

---

## Roadmap

- [ ] GitHub Actions CI: auto-build releases on tag (Linux + Windows cross-compile matrix)
- [ ] Code signing for Windows (eliminate SmartScreen warning on first run)
- [ ] AppImage for Linux (one-click distribution parity with Windows)
- [ ] CERN ROOT support in Windows build (currently blocked by cross-compilation complexity)
- [ ] Bit-equivalence validation against a real large dataset (not just synthetic fixture)
- [ ] Automated regression tests comparing Linux vs Windows outputs in CI

---

## License

MIT — see [LICENSE](LICENSE) for details.

---

## Credits

- **Author**: José Labarca
- **Windows packaging / C++ postprocess port**: Claude (Anthropic)
- **Dependencies**: Qt 6 (LGPL), nlohmann/json (MIT), MinGW-w64, NSIS (zlib license),
  CERN ROOT (LGPL)
