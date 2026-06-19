# BeamLabStudio Python API — Examples

This directory contains Jupyter notebooks demonstrating the Python API.

## Prerequisites

```bash
# Option A: Install from source (with cmake).
cmake -B build -DBEAMLAB_ENABLE_PYTHON=ON
cmake --build build --target pybeamlab
# The module is installed to: build/python/beamlab/
export PYTHONPATH="build/python:$PYTHONPATH"

# Option B: Install via pip (requires pre-built libraries).
pip install beamlab
```

## Notebooks

| Notebook | Description |
|----------|-------------|
| `quickstart.ipynb` | Full tour: materials, particles, Bragg curves, NIST validation, configuration |

## Running

```bash
# Install Jupyter if needed.
pip install jupyter pandas matplotlib

# Launch.
cd src/scripting/examples/
jupyter notebook quickstart.ipynb
```

## What to Try

1. **Material queries**: `mats['water_icru']`, `mats.names()`, `mats.find('lead')`
2. **Particle queries**: `parts.get_by_pdg(2212)`, `parts.names()`
3. **Single-step physics**: `engine.compute_step(energy, step, material, particle)`
4. **Bragg curves**: `engine.compute_bragg_curve(energy, material, particle)`
5. **NIST validation**: `engine.validate_against_nist(material, particle)`
6. **Custom materials**: Create `Material()`, set properties, `mats.add_custom()`
7. **Profiles**: `ProfileManager().resolve('quick_inspect')`

## Troubleshooting

```bash
# Module not found.
python3 -c "import beamlab; beamlab.demo()"

# If that fails, check PYTHONPATH:
export PYTHONPATH=/path/to/BeamLabStudio/build/python:$PYTHONPATH
```
