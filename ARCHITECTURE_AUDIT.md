# Architecture Audit — BeamLabStudio

**Date:** 2026-05-20  
**Status:** Pre-redesign baseline

## Current State

### Directory Structure
```
src/
├── analysis/        # Statistics, focus, envelopes, surfaces (pure C++)
├── app/             # Bootstrap, CLI entry point, service registry
├── biosim/          # New simulation (StoppingPowerEngine, BioSim, materials)
│   ├── core/        # Pipeline, energy conversion
│   ├── geometry/    # Phantoms, slabs
│   ├── materials/   # BioMaterialLibrary, validators
│   ├── physics/     # StoppingPower, Bragg, Straggling, ParticleLibrary
│   └── ui/qt/       # BioSimWidget, viewport, editors
├── core/            # Vec3, Error, NumericGuards, units
├── data/            # Data models: Trajectory, Sample, AxisFrame, etc.
├── geometry/        # Mesh, OBJ parser
├── io/              # Importers, exporters, parsers, detectors
└── ui/qt/           # MainWindow, widgets, scene views
```

### Key Problems

1. **OOM for large datasets (20GB+)**
   - `TrajectoryDataset` holds `std::vector<Trajectory>` which holds `std::vector<TrajectorySample>`.
   - 96M samples × ~120 bytes = ~11.5 GB RAM minimum. Machine has 14 GB.
   - No disk-backed storage, no batch processing.

2. **MainWindow.cpp: 3294 lines** (down from 4445 after refactoring, but still >3000)
   - Still contains: manifest loading, analysis pipeline launch, export UI logic.
   - `openDataFileAndRunWithPath()` is 200+ lines mixing UI, config, and QProcess orchestration.

3. **Bash/QProcess analysis pipeline**
   - Script-based, error-prone (parentheses, paths, env vars).
   - No progress feedback from the CLI to the UI.
   - Requires shell + Python in PATH.

4. **Hardcoded analysis parameters**
   - `defaultAnalysisArguments()` in MainWindow.cpp has magic numbers: `--window 5`, `--caustic-points 256`, etc.
   - No config file support.

5. **Duplication already resolved**
   - `src/simulation/` DELETED. Only `src/biosim/` remains.
   - TissueRegistry DELETED. Only BioMaterialLibrary remains.
   - All F1-F4 tasks from PLAN_DE_ATAQUE completed.

### What Works
- Geant4CsvImporter: streaming input (std::ifstream + std::getline) ✓
- StoppingPowerEngine: Sternheimer correction, MCS Highland ✓  
- BeamLabStudio.qss: single stylesheet source of truth ✓
- Unit tests: 20/20 passing via GoogleTest/ctest ✓
- CI: GitHub Actions build.yml ✓
- Install targets: cmake --install ✓
