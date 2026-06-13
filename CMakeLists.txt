cmake_minimum_required(VERSION 3.20)

include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_Declare(
    sqlite3
    GIT_REPOSITORY https://github.com/azadkuh/sqlite-amalgamation.git
    GIT_TAG 035e7b009d1264ad013f90e09fb030f0275c5ba5
)

project(BeamLabStudio
    VERSION 0.1.0
    DESCRIPTION "Modular scientific platform for beam, trajectory and geometry analysis"
    LANGUAGES CXX C
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ══════════════════════════════════════════════════════════════════════
#  Build options
# ══════════════════════════════════════════════════════════════════════

option(BEAMLAB_ENABLE_QT_UI "Enable BeamLabStudio Qt UI" OFF)
option(BEAMLAB_ENABLE_ROOT "Enable native CERN ROOT importer" OFF)
option(BEAMLAB_ENABLE_PERFORMANCE_TESTS "Build performance benchmarks" ON)
option(BEAMLAB_ENABLE_LTO "Enable Link Time Optimization (Release only)" ON)
option(BEAMLAB_ENABLE_NATIVE_ARCH "Optimize for host CPU (-march=native)" OFF)
option(BEAMLAB_BUILD_PLUGIN_TEMPLATES "Build example plugin .so files" OFF)
option(BEAMLAB_ENABLE_PARQUET "Enable Apache Parquet export (requires Arrow)" OFF)

set(BEAMLAB_PLUGINS_DIR "${CMAKE_INSTALL_PREFIX}/plugins"
    CACHE PATH "Directory where external plugins are installed")

enable_testing()

# ── Platform layer ──────────────────────────────────────────────────

# ── Domain layer (pure C++, no Qt) ──────────────────────────────────

# ── Services layer (orchestration + storage, pure C++) ─────────────

# ── Plugin templates (EXCLUDE_FROM_ALL — not built by default) ─────
if(BEAMLAB_BUILD_PLUGIN_TEMPLATES)
    add_subdirectory(src/plugins EXCLUDE_FROM_ALL)
endif()

add_compile_options(
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wsign-conversion
)

add_library(beamlab_core STATIC
    src/app/ApplicationContext.cpp
    src/app/ApplicationBootstrap.cpp
    src/app/ServiceRegistry.cpp
    src/app/CommandLineOptions.cpp
    src/core/foundation/NumericGuards.cpp
    src/core/config/ConfigLoader.cpp
    src/core/config/ProfileManager.cpp
    src/core/storage/InMemoryStorage.cpp
    src/core/storage/SqliteStorage.cpp
    src/core/pipeline/AnalysisPipeline.cpp
    src/core/storage/InMemoryStorage.cpp
    src/io/detect/FileProbe.cpp
    src/io/detect/ImportRouter.cpp
    src/io/parsing/DelimiterDetector.cpp
    src/io/parsing/TableHeaderAnalyzer.cpp
    src/io/parsing/ColumnTypeInferer.cpp
    src/io/parsing/ComsolHeaderParser.cpp
    src/io/parsing/Geant4HeaderRecognizer.cpp
    src/io/importers/DelimitedTableImporter.cpp
    src/io/importers/ComsolCsvImporter.cpp
    src/io/importers/Geant4CsvImporter.cpp
    src/io/normalization/DatasetNormalizer.cpp
    src/io/normalization/EvolutionAxisResolver.cpp
    src/io/normalization/AxisFrameResolver.cpp
    src/io/exporters/MeshExporter.cpp
    src/io/exporters/ParametricSurfaceExporter.cpp
    src/io/exporters/AnalysisReportExporter.cpp
    src/io/exporters/QualityReportExporter.cpp
    src/io/exporters/VisualizationDataExporter.cpp
    src/io/exporters/VisualizationManifestExporter.cpp
    src/analysis/statistics/FrameStatisticsEngine.cpp
    src/analysis/statistics/BatchStatisticsEngine.cpp
    src/analysis/quality/AnalysisQualityChecker.cpp
    src/analysis/focus/FocusCurveBuilder.cpp
    src/analysis/focus/FocusDetector.cpp
    src/analysis/slice/SlicePlanner.cpp
    src/analysis/slice/SliceProjector.cpp
    src/analysis/slice/SliceExtractor.cpp
    src/analysis/envelope/ConvexHullEnvelopeExtractor.cpp
    src/analysis/envelope/AngularEnvelopeExtractor.cpp
    src/analysis/envelope/FullEnvelopePreviewBuilder.cpp
    src/analysis/surfaces/ContourResampler.cpp
    src/analysis/surfaces/SurfaceBuilder.cpp
    src/analysis/surfaces/LensDiskBuilder.cpp
    # Scoring plane detection
    src/biosim/core/ScoringPlaneDetector.cpp
    # Energy exporters
    src/io/exporters/EnergyProfileExporter.cpp
)

target_include_directories(beamlab_core PUBLIC ${PROJECT_SOURCE_DIR}/src)

FetchContent_MakeAvailable(nlohmann_json)

FetchContent_GetProperties(sqlite3)
if(NOT sqlite3_POPULATED)
    FetchContent_Populate(sqlite3)
    add_library(sqlite3 STATIC ${sqlite3_SOURCE_DIR}/sqlite3.c)
    target_include_directories(sqlite3 PUBLIC ${sqlite3_SOURCE_DIR})
    target_compile_definitions(sqlite3 PRIVATE SQLITE_ENABLE_FTS5 SQLITE_THREADSAFE=1)
endif()

target_link_libraries(beamlab_core PUBLIC nlohmann_json::nlohmann_json sqlite3)
FetchContent_MakeAvailable(nlohmann_json)

target_link_libraries(beamlab_core PUBLIC nlohmann_json::nlohmann_json)

add_executable(beamlab
    src/app/main.cpp
)

target_link_libraries(beamlab PRIVATE beamlab_core)

add_subdirectory(src/domain)

if(BEAMLAB_ENABLE_QT_UI)
    # ── Biological simulator module ─────────────────────────────
    add_library(beamlab_biosim STATIC
        src/biosim/materials/BioMaterialLibrary.cpp
        src/biosim/materials/BioMaterialValidator.cpp
        src/biosim/physics/ParticleLibrary.cpp
        src/biosim/physics/StoppingPowerEngine.cpp
        src/biosim/physics/BraggPeakCalculator.cpp
        src/biosim/physics/EnergyStraggling.cpp
        src/biosim/geometry/PhantomLibrary.cpp
        src/biosim/core/EnergyScaleConverter.cpp
        src/biosim/core/BioSimRunner.cpp
        src/biosim/ui/qt/EnergyColorMapper.cpp
        src/biosim/ui/qt/EnergyScaleBar.cpp
        src/biosim/ui/qt/BioViewport3D.cpp
        src/biosim/ui/qt/TrajectoryInspectorPanel.cpp
        src/biosim/ui/qt/SlabEditor3D.cpp
        src/biosim/ui/qt/BioSimWidget.cpp
        src/biosim/ui/qt/MaterialEditorDialog.cpp
        src/biosim/ui/qt/PhantomPresetPanel.cpp
    )
    target_include_directories(beamlab_biosim PUBLIC ${PROJECT_SOURCE_DIR}/src)
    target_link_libraries(beamlab_biosim PUBLIC Qt6::Widgets Qt6::Concurrent beamlab_core)
    target_compile_features(beamlab_biosim PRIVATE cxx_std_20)
    set_target_properties(beamlab_biosim PROPERTIES AUTOMOC ON)
endif()

if(BEAMLAB_ENABLE_ROOT)
    find_program(ROOT_CONFIG_EXECUTABLE root-config REQUIRED)

    execute_process(
        COMMAND ${ROOT_CONFIG_EXECUTABLE} --incdir
        OUTPUT_VARIABLE ROOT_INCLUDE_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    execute_process(
        COMMAND ${ROOT_CONFIG_EXECUTABLE} --libs
        OUTPUT_VARIABLE ROOT_LIBS_RAW
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    separate_arguments(ROOT_LIBRARIES UNIX_COMMAND "${ROOT_LIBS_RAW}")

    target_sources(beamlab_core PRIVATE
        src/io/importers/RootNativeImporter.cpp
    )

    target_compile_definitions(beamlab_core PUBLIC BEAMLAB_ENABLE_ROOT=1)

    target_include_directories(beamlab_core SYSTEM PRIVATE
        ${ROOT_INCLUDE_DIR}
    )

    target_link_libraries(beamlab_core PUBLIC
        ${ROOT_LIBRARIES}
    )

    set_source_files_properties(
        src/io/importers/RootNativeImporter.cpp
        PROPERTIES COMPILE_OPTIONS "-Wno-cpp;-Wno-pedantic;-Wno-conversion;-Wno-sign-conversion;-Wno-float-conversion"
    )
endif()

if(BEAMLAB_ENABLE_QT_UI)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)
    find_package(Qt6 REQUIRED COMPONENTS Widgets Concurrent)

    # ── Qt UI application ───────────────────────────────────────────
    add_subdirectory(src/ui)
endif()

# ── All tests (unit, integration, performance) ─────────────────────
add_subdirectory(tests)

# ══════════════════════════════════════════════════════════════════════
#  Release optimisation flags (domain/math targets only, not storage)
# ══════════════════════════════════════════════════════════════════════

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    # Apply to domain (physics/math) and core (Vec3, etc.).
    # Do NOT apply to SQLite or storage — those optimise internally.
    foreach(target beamlab_domain beamlab_core)
        target_compile_options(${target} PRIVATE
            $<$<CONFIG:Release>:-O3>
        )
    endforeach()

    if(BEAMLAB_ENABLE_NATIVE_ARCH)
        foreach(target beamlab_domain beamlab_core)
            target_compile_options(${target} PRIVATE -march=native)
        endforeach()
        message(STATUS "Native arch enabled: -march=native")
    endif()

    if(BEAMLAB_ENABLE_LTO)
        set_property(TARGET beamlab_domain
            PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
        set_property(TARGET beamlab_core
            PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
        message(STATUS "LTO enabled (link-time will be 2-3× slower — see docs/BUILD.md)")
    endif()

    if(BEAMLAB_ENABLE_NATIVE_ARCH)
        message(STATUS "Native arch enabled: -march=native")
    endif()
endif()

# ── Python bindings (optional) ────────────────────────────────────

install(TARGETS beamlab RUNTIME DESTINATION bin)
if(BEAMLAB_ENABLE_QT_UI)
    install(TARGETS beamlab_ui
        BUNDLE  DESTINATION .
        RUNTIME DESTINATION bin)
endif()
install(DIRECTORY scripts/ DESTINATION share/BeamLabStudio/scripts
    FILES_MATCHING PATTERN "*.sh" PATTERN "*.py" PATTERN "*.cmd")

