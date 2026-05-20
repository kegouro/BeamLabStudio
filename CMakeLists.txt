cmake_minimum_required(VERSION 3.20)

project(BeamLabStudio
    VERSION 0.1.0
    DESCRIPTION "Modular scientific platform for beam, trajectory and geometry analysis"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wsign-conversion
)

include_directories(${PROJECT_SOURCE_DIR}/src)

add_library(beamlab_core STATIC
    src/app/ApplicationContext.cpp
    src/app/ApplicationBootstrap.cpp
    src/app/ServiceRegistry.cpp
    src/app/CommandLineOptions.cpp
    src/core/foundation/NumericGuards.cpp
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
    # Simulation subsystem
    src/simulation/tissue/TissueRegistry.cpp
    src/simulation/physics/BetheBlochEngine.cpp
    src/simulation/physics/MuonTrackSimulator.cpp
    src/simulation/scoring/ScoringPlaneDetector.cpp
    # Energy exporters
    src/io/exporters/EnergyProfileExporter.cpp
)

target_include_directories(beamlab_core PUBLIC ${PROJECT_SOURCE_DIR}/src)

add_executable(beamlab
    src/app/main.cpp
)

target_link_libraries(beamlab PRIVATE beamlab_core)

option(BEAMLAB_ENABLE_ROOT "Enable native CERN ROOT importer" OFF)

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

option(BEAMLAB_ENABLE_QT_UI "Enable BeamLabStudio Qt UI" OFF)

if(BEAMLAB_ENABLE_QT_UI)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)
    find_package(Qt6 REQUIRED COMPONENTS Widgets Concurrent)

    # Auto-regenerate math_docs.html from //!@math blocks before each UI build.
    # Requires Python 3.  Falls back gracefully if python3 is not found.
    find_program(PYTHON3_EXECUTABLE python3)
    set(MATH_DOCS_SCRIPT "${PROJECT_SOURCE_DIR}/scripts/generate_math_docs.py")
    set(MATH_DOCS_OUTPUT "${PROJECT_SOURCE_DIR}/src/ui/qt/assets/math_docs.html")

    if(PYTHON3_EXECUTABLE)
        add_custom_command(
            OUTPUT  "${MATH_DOCS_OUTPUT}"
            COMMAND "${PYTHON3_EXECUTABLE}" "${MATH_DOCS_SCRIPT}"
            DEPENDS "${MATH_DOCS_SCRIPT}"
                    # Re-run whenever any physics/analysis source changes
                    "${PROJECT_SOURCE_DIR}/src/simulation/physics/BetheBlochEngine.cpp"
                    "${PROJECT_SOURCE_DIR}/src/simulation/scoring/ScoringPlaneDetector.cpp"
                    "${PROJECT_SOURCE_DIR}/src/analysis/statistics/FrameStatisticsEngine.cpp"
                    "${PROJECT_SOURCE_DIR}/src/analysis/focus/FocusDetector.cpp"
                    "${PROJECT_SOURCE_DIR}/src/io/normalization/AxisFrameResolver.cpp"
                    "${PROJECT_SOURCE_DIR}/src/io/normalization/DatasetNormalizer.cpp"
                    "${PROJECT_SOURCE_DIR}/src/io/importers/Geant4CsvImporter.cpp"
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
            COMMENT "Generating math_docs.html from //!@math source blocks"
            VERBATIM
        )
        add_custom_target(math_docs ALL DEPENDS "${MATH_DOCS_OUTPUT}")
    endif()

    # ── Biological simulator module ─────────────────────────────────────────
    # Self-contained; no existing code was modified.
    add_library(beamlab_biosim STATIC
        # Materials
        src/biosim/materials/BioMaterialLibrary.cpp
        src/biosim/materials/BioMaterialValidator.cpp
        # Physics
        src/biosim/physics/ParticleLibrary.cpp
        src/biosim/physics/StoppingPowerEngine.cpp
        src/biosim/physics/BraggPeakCalculator.cpp
        src/biosim/physics/EnergyStraggling.cpp
        # Geometry
        src/biosim/geometry/PhantomLibrary.cpp
        # Core (pipeline)
        src/biosim/core/EnergyScaleConverter.cpp
        src/biosim/core/BioSimRunner.cpp
        # UI widgets (require Qt)
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

    target_link_libraries(beamlab_biosim PUBLIC
        Qt6::Widgets
        Qt6::Concurrent
        beamlab_core
    )

    target_compile_features(beamlab_biosim PRIVATE cxx_std_20)

    add_executable(beamlab_ui
        src/io/exporters/ExportManager.cpp
        src/ui/qt/main.cpp
        src/ui/qt/InteractiveGraphicsView.cpp
        src/ui/qt/MainWindow.cpp
        src/ui/qt/ObjViewerWidget.cpp
        src/ui/qt/RunDashboardWidget.cpp
        src/ui/qt/Scene3DWidget.cpp
        src/ui/qt/InfoWidget.cpp
        src/ui/qt/StatsDashboardWidget.cpp
        src/ui/qt/resources.qrc
    )

    target_include_directories(beamlab_ui PRIVATE
        src
    )

    target_link_libraries(beamlab_ui PRIVATE
        Qt6::Widgets
        Qt6::Concurrent
        beamlab_core
        beamlab_biosim
    )

    target_compile_features(beamlab_ui PRIVATE cxx_std_20)
endif()
