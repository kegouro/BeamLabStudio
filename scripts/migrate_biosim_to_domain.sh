#!/usr/bin/env bash
# ══════════════════════════════════════════════════════════════════════
#  migrate_biosim_to_domain.sh
#  Moves src/biosim/ files to src/domain/, one commit per step.
#  Run from project root:  bash scripts/migrate_biosim_to_domain.sh
# ══════════════════════════════════════════════════════════════════════

set -euo pipefail
cd "$(git rev-parse --show-toplevel 2>/dev/null || echo .)"

echo "══════════════════════════════════════════════════════════════"
echo "  BeamLabStudio — src/biosim/ → src/domain/ Migration"
echo "══════════════════════════════════════════════════════════════"
echo ""

# ──────────── Step 0: Verify clean state ────────────
if ! git diff --quiet; then
    echo "❌ Working tree has uncommitted changes. Commit or stash first."
    exit 1
fi

# ──────────── Step 1: Create destination directories ────────────
echo "▸ Step 1/6: Creating destination directories"
mkdir -p src/domain/physics
mkdir -p src/domain/materials
mkdir -p src/domain/geometry
mkdir -p src/domain/simulation
mkdir -p src/ui/qt/widgets
mkdir -p src/services/analysis/engines
echo "  ✓ Directories created"

# ──────────── Step 2: Move physics files ────────────
echo "▸ Step 2/6: Moving physics files"
git mv src/biosim/physics/StoppingPowerEngine.cpp src/domain/physics/
git mv src/biosim/physics/StoppingPowerEngine.h   src/domain/physics/
git mv src/biosim/physics/EnergyStraggling.cpp    src/domain/physics/
git mv src/biosim/physics/EnergyStraggling.h      src/domain/physics/
git mv src/biosim/physics/BraggPeakCalculator.cpp src/domain/physics/
git mv src/biosim/physics/BraggPeakCalculator.h   src/domain/physics/
echo "  ✓ Physics files moved"
git commit -m "refactor(domain): move biosim/physics/ → domain/physics/"
echo ""

# ──────────── Step 3: Move materials files ────────────
echo "▸ Step 3/6: Moving materials files"
git mv src/biosim/materials/BioMaterialValidator.cpp src/domain/materials/MaterialValidator.cpp
git mv src/biosim/materials/BioMaterialValidator.h   src/domain/materials/MaterialValidator.h
# BioMaterial.h and BioMaterialLibrary.h remain as shims (already replaced by Material.h, MaterialRegistry.h)
echo "  ✓ Materials files moved (shims preserved in biosim/)"
git commit -m "refactor(domain): move biosim/materials/ → domain/materials/"
echo ""

# ──────────── Step 4: Move geometry files ────────────
echo "▸ Step 4/6: Moving geometry files"
git mv src/biosim/geometry/BioSlab.h              src/domain/geometry/Slab.h
git mv src/biosim/geometry/PhantomLibrary.cpp      src/domain/geometry/
git mv src/biosim/geometry/PhantomLibrary.h        src/domain/geometry/
git mv src/biosim/geometry/PhantomPreset.h         src/domain/geometry/
echo "  ✓ Geometry files moved"
git commit -m "refactor(domain): move biosim/geometry/ → domain/geometry/"
echo ""

# ──────────── Step 5: Move core/simulation files ────────────
echo "▸ Step 5/6: Moving core files"
git mv src/biosim/core/BioSimRunner.cpp          src/domain/simulation/
git mv src/biosim/core/BioSimRunner.h            src/domain/simulation/
git mv src/biosim/core/BioSimConfig.h            src/domain/simulation/
git mv src/biosim/core/BioSimResult.h            src/domain/simulation/
git mv src/biosim/core/EnergyScaleConverter.cpp  src/domain/simulation/
git mv src/biosim/core/EnergyScaleConverter.h    src/domain/simulation/
git mv src/biosim/core/EnergyScaleSet.h          src/domain/simulation/
git mv src/biosim/core/ScoringPlaneDetector.cpp  src/services/analysis/engines/
git mv src/biosim/core/ScoringPlaneDetector.h    src/services/analysis/engines/
git mv src/biosim/core/ScoringPlane.h            src/services/analysis/engines/
echo "  ✓ Core files moved"
git commit -m "refactor(domain): move biosim/core/ → domain/simulation/ and services/analysis/engines/"
echo ""

# ──────────── Step 6: Move UI widgets ────────────
echo "▸ Step 6/6: Moving UI widgets"
git mv src/biosim/ui/qt/EnergyColorMapper.cpp         src/ui/qt/widgets/
git mv src/biosim/ui/qt/EnergyColorMapper.h           src/ui/qt/widgets/
git mv src/biosim/ui/qt/EnergyScaleBar.cpp            src/ui/qt/widgets/
git mv src/biosim/ui/qt/EnergyScaleBar.h              src/ui/qt/widgets/
git mv src/biosim/ui/qt/BioViewport3D.cpp             src/ui/qt/widgets/
git mv src/biosim/ui/qt/BioViewport3D.h               src/ui/qt/widgets/
git mv src/biosim/ui/qt/TrajectoryInspectorPanel.cpp  src/ui/qt/widgets/
git mv src/biosim/ui/qt/TrajectoryInspectorPanel.h    src/ui/qt/widgets/
git mv src/biosim/ui/qt/SlabEditor3D.cpp              src/ui/qt/widgets/
git mv src/biosim/ui/qt/SlabEditor3D.h                src/ui/qt/widgets/
git mv src/biosim/ui/qt/BioSimWidget.cpp              src/ui/qt/widgets/
git mv src/biosim/ui/qt/BioSimWidget.h                src/ui/qt/widgets/
git mv src/biosim/ui/qt/MaterialEditorDialog.cpp      src/ui/qt/widgets/
git mv src/biosim/ui/qt/MaterialEditorDialog.h        src/ui/qt/widgets/
git mv src/biosim/ui/qt/PhantomPresetPanel.cpp        src/ui/qt/widgets/
git mv src/biosim/ui/qt/PhantomPresetPanel.h          src/ui/qt/widgets/
echo "  ✓ UI widget files moved"
git commit -m "refactor(domain): move biosim/ui/qt/ → src/ui/qt/widgets/"
echo ""

# ──────────── Summary ────────────
echo "══════════════════════════════════════════════════════════════"
echo "  Migration complete.  Files moved in 6 git commits."
echo ""
echo "  🟢  src/biosim/physics/   → src/domain/physics/"
echo "  🟢  src/biosim/materials/ → src/domain/materials/"
echo "  🟢  src/biosim/geometry/  → src/domain/geometry/"
echo "  🟢  src/biosim/core/      → src/domain/simulation/"
echo "  🟢  src/biosim/core/      → src/services/analysis/engines/"
echo "  🟢  src/biosim/ui/qt/     → src/ui/qt/widgets/"
echo ""
echo "  📌  SHIMS preserved in src/biosim/ for transition:"
echo "       - biosim/materials/BioMaterial.h"
echo "       - biosim/materials/BioMaterialLibrary.h"
echo "       - biosim/physics/ParticleLibrary.h"
echo "       - biosim/physics/StoppingPowerEngine.h"
echo ""
echo "  NEXT STEPS:"
echo "   1. Update include paths in all files that #include old biosim/ paths"
echo "   2. Run cmake -B build && cmake --build build"
echo "   3. Fix any compilation errors"
echo "   4. Remove shim headers (after 1-2 sprints)"
echo "══════════════════════════════════════════════════════════════"
