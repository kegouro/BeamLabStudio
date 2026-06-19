from setuptools import setup, Extension
from pybind11.setup_helpers import Pybind11Extension, build_ext
import sys
import os

# ── BeamLabStudio Python bindings ──────────────────────────────────
#
# Two build methods:
#   1. pip install .              — standalone build (requires pybind11 + libs)
#   2. cmake -DBEAMLAB_ENABLE_PYTHON=ON  — integrated build
#
# The cmake method is preferred because it handles all library dependencies.
# This setup.py is provided for pip-based builds in CI.

beamlab_root = os.path.dirname(os.path.abspath(__file__))

ext_modules = [
    Pybind11Extension(
        "beamlab._core",
        [os.path.join(beamlab_root, "src", "scripting", "pybind11", "beamlab_module.cpp")],
        include_dirs=[
            os.path.join(beamlab_root, "src"),
        ],
        library_dirs=[
            os.path.join(beamlab_root, "build", "src", "domain"),
            os.path.join(beamlab_root, "build", "src", "services"),
            os.path.join(beamlab_root, "build", "src", "core"),
            os.path.join(beamlab_root, "build", "src", "platform"),
        ],
        libraries=[
            "beamlab_domain",
            "beamlab_services",
            "beamlab_core",
            "beamlab_platform",
        ],
        extra_compile_args=["-std=c++17", "-O2"],
        runtime_library_dirs=["."],
        cxx_std=17,
    ),
]

setup(
    name="beamlab",
    version="3.0.0",
    description="BeamLabStudio — scientific analysis library for muon beam trajectory data",
    long_description=open(os.path.join(beamlab_root, "README.md"), encoding="utf-8").read(),
    long_description_content_type="text/markdown",
    author="José Labarca",
    author_email="",
    url="https://github.com/kegouro/BeamLabStudio",
    license="MIT",
    ext_modules=ext_modules,
    cmdclass={"build_ext": build_ext},
    zip_safe=False,
    python_requires=">=3.8",
    install_requires=[
        "numpy>=1.20",
        "matplotlib>=3.4",
    ],
    classifiers=[
        "Development Status :: 4 - Beta",
        "Intended Audience :: Science/Research",
        "License :: OSI Approved :: MIT License",
        "Programming Language :: C++",
        "Programming Language :: Python :: 3",
        "Topic :: Scientific/Engineering :: Physics",
        "Topic :: Scientific/Engineering :: Medical Science Apps.",
    ],
)
