# ══════════════════════════════════════════════════════════════════════
#  CMakeLists.txt — Example BeamLabStudio Import Plugin
#
#  This CMake project builds a shared library (.so / .dylib / .dll)
#  that BeamLabStudio can load at runtime via PluginHost.
#
#  Usage:
#    mkdir build && cd build
#    cmake .. -DCMAKE_PREFIX_PATH=/path/to/BeamLabStudio/build
#    cmake --build .
#    cmake --install . --prefix ~/.config/BeamLabStudio/plugins
# ══════════════════════════════════════════════════════════════════════

cmake_minimum_required(VERSION 3.20)

# ── Project ────────────────────────────────────────────────────────
# Change the project name to match your plugin.
project(ExampleImporterPlugin
    VERSION 1.0.0
    DESCRIPTION "Example import plugin for BeamLabStudio"
    LANGUAGES CXX
)

# ── C++ standard ──────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Find BeamLabStudio SDK ────────────────────────────────────────
# Three ways to point CMake at a BeamLabStudio installation:
#
#   1. CMAKE_PREFIX_PATH   (recommended):
#        cmake .. -DCMAKE_PREFIX_PATH=/path/to/BeamLabStudio/build
#
#   2. BEAMLAB_DIR:
#        cmake .. -DBEAMLAB_DIR=/path/to/BeamLabStudio/build
#
#   3. System install:
#        cmake .. -DCMAKE_INSTALL_PREFIX=/usr
#
# The find module looks for:
#   - BeamLabStudioConfig.cmake  (from the build tree)
#   - beamlab_services_analysis  (the services library)
#   - beamlab_domain             (the domain library)
#   - beamlab_core               (the core library)

# ── Configurable location ──────────────────────────────────────────
# Allow the user to override with -DBEAMLAB_DIR=...
if(NOT BEAMLAB_DIR)
    set(BEAMLAB_DIR "$ENV{BEAMLAB_DIR}" CACHE PATH "BeamLabStudio installation prefix")
endif()

if(BEAMLAB_DIR)
    list(APPEND CMAKE_PREFIX_PATH "${BEAMLAB_DIR}")
endif()

# ── Create a config file package if BeamLabStudio hasn't been installed ──
# If BeamLabStudioConfig.cmake doesn't exist, we fall back to finding
# the individual libraries directly.
find_path(BEAMLAB_SDK_INCLUDE_DIR
    NAMES beamlab/services/import/IImporter.h
    PATHS
        ${BEAMLAB_DIR}/include
        ${BEAMLAB_DIR}/../src
        /usr/include
        /usr/local/include
)

# ── Shared library target ──────────────────────────────────────────
# Change "example_importer" to match your plugin name.
#
# BUILD_SHARED_LIBS or add_library(... SHARED) must be used so the
# plugin is a .so/.dylib/.dll that can be dlopen'd by PluginHost.
add_library(example_importer SHARED
    ExampleImporter.cpp
)

# ── Include directories ─────────────────────────────────────────────
target_include_directories(example_importer PRIVATE
    ${BEAMLAB_SDK_INCLUDE_DIR}
)

# ── Link BeamLabStudio libraries ────────────────────────────────────
# The plugin needs at minimum:
#   - beamlab_services_analysis  (IImporter interface)
#   - beamlab_domain             (TrajectorySample, Material types)
#   - beamlab_core               (Vec3, math helpers)
#
# If the find module is available, use:
#   find_package(BeamLabStudio REQUIRED COMPONENTS services)
#   target_link_libraries(example_importer PRIVATE BeamLabStudio::services)
#
# Otherwise, link directly:
if(TARGET beamlab_services_analysis)
    target_link_libraries(example_importer PRIVATE
        beamlab_services_analysis
        beamlab_domain
        beamlab_core
    )
else()
    # Fallback: assume headers only are sufficient (IImporter is pure virtual).
    # The actual symbols are resolved at dlopen time from the host process.
    target_compile_definitions(example_importer PRIVATE BEAMLAB_PLUGIN_HEADER_ONLY)
endif()

# ── Compiler flags ──────────────────────────────────────────────────
target_compile_options(example_importer PRIVATE
    -Wall -Wextra -Wpedantic
    -fvisibility=hidden          # Only exported symbols are visible
    -fvisibility-inlines-hidden  # Inline functions are never part of ABI
)

# ── Output naming ──────────────────────────────────────────────────
# The output filename should match what PluginHost expects.
# PluginHost scans for .so/.dylib/.dll files and dlopens them.
set_target_properties(example_importer PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins/importers"
    PREFIX ""                  # No "lib" prefix for the .so
)

# ── Install ─────────────────────────────────────────────────────────
# Default installation path: ~/.config/BeamLabStudio/plugins/importers/
# PluginHost scans this directory at startup.
#
# Override with:
#   cmake --install . --prefix ~/my-custom-path
#   cmake .. -DCMAKE_INSTALL_PREFIX=/path/to/BeamLabStudio
#
if(NOT CMAKE_INSTALL_PREFIX)
    set(CMAKE_INSTALL_PREFIX "$ENV{HOME}/.config/BeamLabStudio/plugins")
endif()

install(TARGETS example_importer
    LIBRARY DESTINATION importers
    RUNTIME DESTINATION importers  # Windows .dll
)
