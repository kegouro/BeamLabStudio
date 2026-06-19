# BeamLabStudio Import Plugin — ExampleImporter

This directory contains a fully documented template for creating a
dynamic import plugin for BeamLabStudio.  Use it as a starting point
to add support for your own data format.

## Quick Start

```bash
# 1. Point CMake at a BeamLabStudio build tree.
export BEAMLAB_DIR=/path/to/BeamLabStudio/build-ui

# 2. Configure and build.
mkdir build && cd build
cmake .. -DCMAKE_PREFIX_PATH=$BEAMLAB_DIR
cmake --build .

# 3. Install the plugin to the user plugin directory.
cmake --install . --prefix ~/.config/BeamLabStudio/plugins

# 4. Verify the plugin was installed.
ls -la ~/.config/BeamLabStudio/plugins/importers/example_importer.so
```

## File Structure

```
template/
├── CMakeLists.txt        # Build system
├── ExampleImporter.h     # Public header — adapt name(), probe(), import()
├── ExampleImporter.cpp   # Implementation — adapt probe logic + parse loop
└── README.md             # This file
```

## How It Works

### 1. Detection (`probe()`)

When the user selects a file to import, BeamLabStudio calls `probe()`
on every registered importer.  The importer with the highest score wins.

| Score | Meaning |
|-------|---------|
| 1.0   | Exact magic bytes match (most reliable) |
| 0.9   | Exact header text match |
| 0.7   | Column headers match expected names |
| 0.5   | Structural clues (e.g. looks like CSV with 5 columns) |
| 0.3   | Extension-only match (weak — use as fallback) |
| 0.0   | Not our format |

**Recommendation:** Use magic bytes or a unique header string for
scores ≥ 0.9.  Relying only on file extension can lead to false
positives when users import `.csv` or `.txt` files from other tools.

### 2. Import (`import()`)

The winning importer's `import()` method is called with:
- The file path
- An `IStorageBackend` to write samples into
- An optional progress callback

Your implementation must:

1. Call `storage.beginWriteBatch()` before writing the first sample.
2. Wrap the import loop in an RAII guard that calls
   `storage.endWriteBatch()` on destruction (even if an exception occurs).
3. Parse your format row-by-row and call `storage.writeSample()` for each.
4. Call `onProgress(bytesRead, totalBytes)` periodically so the UI
   can display a progress bar and ETA.

### 3. Registration (`create_importer()`)

BeamLabStudio's `PluginHost` uses `dlopen()` to load your `.so` file,
then calls `dlsym()` to find the `create_importer` symbol.  This
function must:

- Be declared `extern "C"` (no C++ name mangling)
- Have default visibility (`__attribute__((visibility("default")))`)
- Return a raw `IImporter*` pointer (ownership transfers to the host)

```cpp
extern "C" __attribute__((visibility("default")))
beamlab::services::import::IImporter* create_importer() {
    return new MyImporter();
}
```

## Adapting the Template

1. **Copy** this directory to a new location:
   ```bash
   cp -r template ~/my_importer
   ```

2. **Rename** the files and classes:
   - `ExampleImporter.h`  → `MyImporter.h`
   - `ExampleImporter.cpp` → `MyImporter.cpp`
   - `class ExampleImporter` → `class MyImporter`
   - Project name in `CMakeLists.txt` → `MyImporterPlugin`

3. **Implement** `probe()`:
   - Read the first 4–8 bytes of the file
   - Compare against your format's magic number or header text
   - Return a score 0.0–1.0

4. **Implement** `import()`:
   - Open the file
   - Loop over rows/records
   - Convert each record to a `beamlab::data::TrajectorySample`
   - Call `storage.writeSample(sample)`
   - Call `onProgress()` every N samples

5. **Build** and **install** (see Quick Start above).

## Testing Your Plugin

### Automated test

Create a small test binary that links against BeamLabStudio's test
utilities and verifies your probe + import logic:

```cpp
#include <gtest/gtest.h>
#include "MyImporter.h"

TEST(MyImporterTest, ProbeRecognisesValidFile) {
    // Create a temp file with your format's magic bytes.
    // Verify probe() returns score ≥ 0.9.
}
```

### Manual test

1. Build BeamLabStudio with `BEAMLAB_ENABLE_QT_UI=ON`.
2. Launch the UI: `./build-ui/beamlab_ui`
3. Open a file in your format via **File → Open**.
4. Check the status bar for import progress.
5. Verify the imported data appears in the 3D viewport and dashboard.

## Installation Paths

PluginHost scans these directories at startup (in order):

| Platform | Path |
|----------|------|
| Linux    | `~/.config/BeamLabStudio/plugins/importers/` |
| Linux    | `<install-prefix>/share/BeamLabStudio/plugins/importers/` |
| macOS    | `~/Library/Application Support/BeamLabStudio/plugins/importers/` |
| Windows  | `%APPDATA%/BeamLabStudio/plugins/importers/` |

## Troubleshooting

### "The plugin is not listed in the Importer Registry"

```bash
# 1. Verify the plugin file exists.
ls -la ~/.config/BeamLabStudio/plugins/importers/

# 2. Check that all dependencies are resolvable.
ldd ~/.config/BeamLabStudio/plugins/importers/example_importer.so

# 3. Run BeamLabStudio with verbose logging.
BEAMLAB_PLUGIN_DEBUG=1 ./beamlab_ui 2>&1 | grep -i plugin
```

### "`plugin->initialize()` crashed"

- Verify your plugin does not throw in the constructor or `probe()`.
- Check that your `create_importer()` returns a valid pointer (not `nullptr`).

### "Symbol `create_importer` not found"

```bash
# Check that the symbol is exported.
nm -D ~/.config/BeamLabStudio/plugins/importers/example_importer.so | grep create_importer

# Expected output:  T create_importer
# If missing, verify extern "C" and -fvisibility=hidden are set correctly.
```

### "Segfault on import"

- Ensure you call `storage.beginWriteBatch()` before writing samples.
- Ensure your RAII guard calls `storage.endWriteBatch()`.
- Verify that `TrajectorySample` fields are populated correctly (especially
  `trajectory_id` — it must be non-zero).

## API Reference

### Classes your plugin uses

| Class | Header | Purpose |
|-------|--------|---------|
| `beamlab::services::import::IImporter` | `<beamlab/services/import/IImporter.h>` | Interface your plugin implements |
| `beamlab::services::storage::IStorageBackend` | `<beamlab/services/storage/IStorageBackend.h>` | Backend you write samples into |
| `beamlab::data::TrajectorySample` | `<beamlab/data/model/TrajectorySample.h>` | Sample struct: position_m, edep_MeV, kinE_MeV, trajectory_id |
| `beamlab::data::TrajectoryId` | `<beamlab/data/ids/TrajectoryId.h>` | Strongly-typed trajectory ID |
| `beamlab::services::import::ImporterCapabilityScore` | `<beamlab/services/import/IImporter.h>` | Probe result: `{value, matchedHeader}` |

### Progress callback signature

```cpp
using ImportProgressCallback = std::function<void(
    uint64_t bytesRead,      // Bytes processed so far
    uint64_t totalBytes      // Total file size (0 if unknown)
)>;
```

---

*For more details, see `docs/PLUGIN_DEVELOPMENT.md` in the BeamLabStudio
repository and the `PluginHost` implementation in `src/platform/`.*
