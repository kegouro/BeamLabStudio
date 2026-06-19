# ══════════════════════════════════════════════════════════════════════
#  BeamLabStudio Python bindings (pybind11)
#  Build with:  cmake -B build -DBEAMLAB_ENABLE_PYTHON=ON
#  Test:        python3 -c "import beamlab; beamlab.demo()"
# ══════════════════════════════════════════════════════════════════════

option(BEAMLAB_ENABLE_PYTHON "Build Python bindings with pybind11" OFF)

if(BEAMLAB_ENABLE_PYTHON)
    # ── Python interpreter ─────────────────────────────────────────
    find_package(Python3 REQUIRED COMPONENTS Interpreter Development)

    # ── pybind11 ───────────────────────────────────────────────────
    find_package(pybind11 QUIET)
    if(NOT pybind11_FOUND)
        include(FetchContent)
        FetchContent_Declare(
            pybind11
            GIT_REPOSITORY https://github.com/pybind/pybind11.git
            GIT_TAG v2.12.0
        )
        FetchContent_MakeAvailable(pybind11)
    endif()

    # ── Python module ──────────────────────────────────────────────
    pybind11_add_module(pybeamlab MODULE
        pybind11/beamlab_module.cpp
    )

    # ── Include paths ─────────────────────────────────────────────
    target_include_directories(pybeamlab PRIVATE
        ${PROJECT_SOURCE_DIR}/src
    )

    # ── Link BeamLabStudio libraries ──────────────────────────────
    target_link_libraries(pybeamlab PRIVATE
        beamlab_core
        beamlab_domain
        beamlab_platform
        beamlab_services
        nlohmann_json::nlohmann_json
        sqlite3
    )

    # ── RPATH for standalone wheel ─────────────────────────────────
    # Embed rpath so the .so finds its dependencies without LD_LIBRARY_PATH.
    set_target_properties(pybeamlab PROPERTIES
        OUTPUT_NAME _core
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/python/beamlab
        INSTALL_RPATH "$ORIGIN"
        BUILD_WITH_INSTALL_RPATH TRUE
    )

    # ── C++ standard ──────────────────────────────────────────────
    target_compile_features(pybeamlab PRIVATE cxx_std_20)

    # ── Install ───────────────────────────────────────────────────
    install(TARGETS pybeamlab
        DESTINATION ${Python3_SITELIB}/beamlab
    )

    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/__init__.py
        DESTINATION ${Python3_SITELIB}/beamlab
    )

    message(STATUS "Python bindings enabled: ${Python3_SITELIB}/beamlab/")
endif()
