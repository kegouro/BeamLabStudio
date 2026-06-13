# ══════════════════════════════════════════════════════════════════════
#  beamlab_ui — Qt desktop application
#  Requires Qt6 (Widgets + Concurrent), AUTOMOC and AUTORCC enabled.
# ══════════════════════════════════════════════════════════════════════

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Concurrent)

# ── Math docs auto-generation ──────────────────────────────────────
find_program(PYTHON3_EXECUTABLE python3)
set(MATH_DOCS_SCRIPT "${PROJECT_SOURCE_DIR}/scripts/generate_math_docs.py")
set(MATH_DOCS_OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/qt/assets/math_docs.html")

if(PYTHON3_EXECUTABLE)
    add_custom_command(
        OUTPUT  "${MATH_DOCS_OUTPUT}"
        COMMAND "${PYTHON3_EXECUTABLE}" "${MATH_DOCS_SCRIPT}"
        DEPENDS "${MATH_DOCS_SCRIPT}"
                "${PROJECT_SOURCE_DIR}/src/biosim/core/ScoringPlaneDetector.cpp"
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

# ── Main executable ────────────────────────────────────────────────
add_executable(beamlab_ui
    # Entry point
    qt/main.cpp

    # Core UI — Qt widgets (root level)
    qt/MainWindow.cpp
    qt/InfoWidget.cpp
    qt/InteractiveGraphicsView.cpp
    qt/ObjViewerWidget.cpp
    qt/RunDashboardWidget.cpp
    qt/Scene3DWidget.cpp
    qt/StatsDashboardWidget.cpp

    # Sub-widgets (migrated from biosim)

    # Shell (command palette backend — Qt-free logic)
    qt/shell/ActionRegistry.cpp

    # Presenters
    qt/presenters/AnalysisPresenter.cpp

    # Dockable system

    # External IO (Qt-specific)
    ${PROJECT_SOURCE_DIR}/src/io/exporters/ExportManager.cpp

    # Resources
    qt/resources.qrc
)

# ── macOS .app bundle ──────────────────────────────────────────────
if(APPLE)
    set(BEAMLAB_ICNS "${PROJECT_SOURCE_DIR}/resources/macos/AppIcon.icns")
    set_source_files_properties("${BEAMLAB_ICNS}" PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources")
    target_sources(beamlab_ui PRIVATE "${BEAMLAB_ICNS}")
    set_target_properties(beamlab_ui PROPERTIES
        MACOSX_BUNDLE ON
        OUTPUT_NAME "BeamLabStudio"
        MACOSX_BUNDLE_INFO_PLIST "${PROJECT_SOURCE_DIR}/resources/macos/Info.plist.in"
        MACOSX_BUNDLE_BUNDLE_NAME "BeamLabStudio"
        MACOSX_BUNDLE_GUI_IDENTIFIER "studio.beamlab.app"
        MACOSX_BUNDLE_ICON_FILE "AppIcon")
endif()

# ── Views (they are compiled with beamlab_ui since they depend on Qt) ──
target_sources(beamlab_ui PRIVATE
)

# ── Include paths ──────────────────────────────────────────────────
target_include_directories(beamlab_ui PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# ── Link dependencies ──────────────────────────────────────────────
target_link_libraries(beamlab_ui PRIVATE
    Qt6::Widgets
    Qt6::Concurrent
    beamlab_core
    beamlab_biosim
)

target_compile_features(beamlab_ui PRIVATE cxx_std_20)
