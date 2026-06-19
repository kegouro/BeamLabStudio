# ══════════════════════════════════════════════════════════════════════
#  pybeamlab — BeamLabStudio Python bindings (pybind11)
#
#  Build with:  cmake -B build -DBEAMLAB_ENABLE_PYTHON=ON
#  Test:        python3 -c "import beamlab; beamlab.demo()"
#
#  Requires:    pybind11 (fetched automatically)
#  No Qt dependency.
# ══════════════════════════════════════════════════════════════════════

option(BEAMLAB_ENABLE_PYTHON "Build Python bindings with pybind11" OFF)

if(BEAMLAB_ENABLE_PYTHON)
    # ── Detect Python interpreter ─────────────────────────────────
    find_package(Python3 REQUIRED COMPONENTS Interpreter Development)

    # ── Fetch pybind11 ────────────────────────────────────────────
    include(FetchContent)
    FetchContent_Declare(
        pybind11
        GIT_REPOSITORY https://github.com/pybind/pybind11.git
        GIT_TAG v2.12.0
    )
    FetchContent_MakeAvailable(pybind11)

    # ── Build the Python module as a shared library ───────────────
    pybind11_add_module(beamlab_py MODULE
        beamlab_module.cpp
    )

    # ── Include paths ─────────────────────────────────────────────
    target_include_directories(beamlab_py PRIVATE
        ${PROJECT_SOURCE_DIR}/src
    )

    # ── Link BeamLabStudio libraries ──────────────────────────────
    target_link_libraries(beamlab_py PRIVATE
        beamlab_core
        beamlab_domain
        beamlab_platform
        nlohmann_json::nlohmann_json
    )

    # ── C++ standard ──────────────────────────────────────────────
    target_compile_features(beamlab_py PRIVATE cxx_std_20)

    # ── Output naming ─────────────────────────────────────────────
    set_target_properties(beamlab_py PROPERTIES
        OUTPUT_NAME "beamlab"
        PREFIX ""
        SUFFIX ".so"
    )

    # ── Install ───────────────────────────────────────────────────
    install(TARGETS beamlab_py
        DESTINATION ${Python3_SITELIB}
    )

    message(STATUS "Python bindings enabled. Module: ${Python3_SITELIB}/beamlab.so")
endif()
