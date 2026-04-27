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
    src/io/detect/FileProbe.cpp
    src/io/detect/ImportRouter.cpp
    src/io/parsing/DelimiterDetector.cpp
    src/io/parsing/TableHeaderAnalyzer.cpp
    src/io/parsing/ColumnTypeInferer.cpp
    src/io/parsing/ComsolHeaderParser.cpp
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
    src/analysis/surfaces/ContourResampler.cpp
    src/analysis/surfaces/SurfaceBuilder.cpp
    src/analysis/surfaces/LensDiskBuilder.cpp
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
    set(CMAKE_AUTORCC ON)
    find_package(Qt6 REQUIRED COMPONENTS Widgets)

    add_executable(beamlab_ui
        src/ui/qt/main.cpp
        src/ui/qt/InteractiveGraphicsView.cpp
        src/ui/qt/MainWindow.cpp
        src/ui/qt/ObjViewerWidget.cpp
        src/ui/qt/RunDashboardWidget.cpp
        src/ui/qt/Scene3DWidget.cpp
        src/ui/qt/StatsDashboardWidget.cpp
        src/ui/qt/resources.qrc
    )

    target_include_directories(beamlab_ui PRIVATE
        src
    )

    target_link_libraries(beamlab_ui PRIVATE
        Qt6::Widgets
    )

    target_compile_features(beamlab_ui PRIVATE cxx_std_20)
endif()
