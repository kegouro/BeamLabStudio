# Contributing to BeamLabStudio

> PRs are welcome. If you find a bug, please fix it. Probably the author is already in Germany.
>
> — José Labarca

---

## Quick Start

```bash
# Prerequisites: cmake 3.20+, g++/clang++, Qt6 (for UI), Python 3 (for docs)
git clone https://github.com/kegouro/BeamLabStudio
cd BeamLabStudio

# Debug build (default)
cmake -B build -DBEAMLAB_ENABLE_QT_UI=ON
cmake --build build -j$(nproc)

# Run tests
ctest --test-dir build --output-on-failure -j$(nproc)
```

## Build Options

| Flag | Default | Description |
|------|---------|-------------|
| `BEAMLAB_ENABLE_QT_UI` | OFF | Build Qt6 desktop application |
| `BEAMLAB_ENABLE_ROOT` | OFF | Enable CERN ROOT file import |
| `BEAMLAB_ENABLE_PERFORMANCE_TESTS` | ON | Build performance benchmarks |
| `BEAMLAB_ENABLE_LTO` | ON | Link-time optimisation (Release) |
| `BEAMLAB_ENABLE_NATIVE_ARCH` | OFF | `-march=native` (non-distributable) |
| `BEAMLAB_BUILD_PLUGIN_TEMPLATES` | OFF | Build example plugin .so |

## Git Workflow

1. **Fork** the repository on GitHub.
2. **Branch**: create a branch with a descriptive name:
   ```bash
   git checkout -b feature/my-feature
   # or
   git checkout -b fix/bug-description
   ```
3. **Commit** using Conventional Commits:
   ```
   type(scope): description

   feat(importer): add HDF5 support
   fix(storage): handle division by zero in FrameStatistics
   refactor(domain): move biosim/ to domain/
   test(platform): add EventBus unit tests
   docs(arch): update Fase 6 checklist
   ```
4. **Push** and create a **Pull Request** against `main`.
5. **CI must pass** before merge.

## Code Style

BeamLabStudio follows a modified [Google C++ Style](https://google.github.io/styleguide/cppguide.html).

### Formatting

Run clang-format before every commit:

```bash
find src/ tests/ -name '*.cpp' -o -name '*.h' | xargs clang-format-18 -i
git diff --stat
```

The project's `.clang-format` enforces:
- 4-space indentation, 100-column limit
- Left-aligned pointers (`int* p` not `int *p`)
- Includes sorted and grouped by layer: platform → services → domain → core → stdlib
- Braces on new lines for classes, functions, methods

### Naming Conventions

```cpp
// Files and classes: PascalCase
class SimulationEngine {};

// Namespaces: snake_case
namespace beamlab::domain::physics {}

// Methods and variables: camelCase
double computeStep(double kinE_MeV);

// Constants: kPrefixed PascalCase
constexpr double k_BB = 0.307075;

// Macros: UPPER_SNAKE_CASE
#pragma once
#define BEAMLAB_ENABLE_ROOT 1
```

### Architecture Rules

| Rule | Rationale |
|------|-----------|
| **No Qt in `domain/` or `services/`** | These layers must be pure C++17, testable without `QApplication` |
| **No singletons** | Use dependency injection via constructor → `ServiceRegistry` |
| **No `setStyleSheet()` inline** | Theme via global `beamlabstudio.qss` only |
| **RAII for resources** | Use scope guards for `endWriteBatch()`, file handles, temp dirs |
| **`[[nodiscard]]` on all value-returning functions** | Catches unused results at compile time |

## Testing

All new code must have tests.

### Test Structure

```
tests/
├── unit/                          # GoogleTest unit tests
│   ├── domain/                    # MaterialRegistry, ParticleRegistry, SimulationEngine
│   ├── platform/                  # EventBus, CommandBus, PluginHost
│   └── services/                  # ImporterRegistry, ExporterRegistry, QueryCache...
├── integration/                   # End-to-end pipeline tests
│   └── test_full_pipeline.cpp
└── performance/                   # Benchmarks (labeled [performance])
    └── bench_*.cpp
```

### Running Tests

```bash
# All unit tests
ctest --test-dir build --output-on-failure

# Specific test group
ctest --test-dir build -R "MaterialRegistry|ParticleRegistry" --verbose

# Integration tests
ctest --test-dir build -L integration --verbose

# Performance benchmarks
ctest --test-dir build -L performance --verbose

# Single test with full output
./build/beamlab_tests --gtest_filter="EventBusTest.PublishSubscribe"
```

### Coverage

```bash
cmake -B build -DENABLE_COVERAGE=ON
cmake --build build --target coverage
firefox build/coverage/index.html
```

## Code Review Checklist

Before requesting a review, verify:

- [ ] Builds without warnings (`-Wall -Wextra -Wpedantic`)
- [ ] New tests pass (`ctest -R my_test`)
- [ ] No Qt includes in domain/ or services/ layers
- [ ] All resources use RAII (no manual `new`/`delete`)
- [ ] Dependency injection via constructor, not singletons
- [ ] `[[nodiscard]]` on all value-returning functions
- [ ] `const` correctness: methods that don't mutate are marked `const`
- [ ] `override` keyword on all overridden virtual functions
- [ ] No magic numbers — use `constexpr` or config JSON
- [ ] No `setStyleSheet()` inline — use QSS theme
- [ ] No `QProcess` for pipeline tasks — use `AnalysisOrchestrator`
- [ ] Commit message follows Conventional Commits format
- [ ] clang-format was run on all changed files

## Project Layers

```
beamlab_ui (Qt6)
  └── UI widgets, Views, Presenters (QObject)

beamlab_services_import/export/analysis (C++17)
  └── ImporterRegistry, ExporterRegistry, Orchestrator, storage

beamlab_domain (C++17)
  └── Physics, Materials, Geometry, Simulation models

beamlab_core (C++17)
  └── Math (Vec3), Foundation (Error), Config, IO

beamlab_platform (C++17)
  └── EventBus, CommandBus, PluginHost, ServiceRegistry
```

Each layer may only depend on layers below it:

```
ui → services → domain → core
                 domain → platform
```

## Documentation

- `docs/ARCHITECTURE.md` — Architecture decisions (ADRs), design rationale
- `docs/PLUGIN_DEVELOPMENT.md` — How to write import/export/analysis plugins
- `docs/BLUEPRINT.md` — Long-term roadmap and phase planning
- `docs/checklist_fase*.md` — Phase-specific validation checklists
- `README.md` — Project overview, build instructions, badge dashboard

Generate math documentation from source code:

```bash
python3 scripts/generate_math_docs.py
```

## Questions?

Open a GitHub Issue or reach out to the maintainer.
