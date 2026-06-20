FetchContent_MakeAvailable(googletest)
include(GoogleTest)

add_executable(beamlab_tests
    test_FrameStatisticsEngine.cpp
    test_FocusDetector.cpp
    test_Geant4CsvImporter.cpp
    test_SurfaceBuilder.cpp
    test_SampleStorage.cpp
    test_BatchStatisticsEngine.cpp
    test_PhysicsRegression.cpp
    test_PhysicsRegressionFixes.cpp
    test_F2_fixes.cpp
    # ── Domain layer (F0-F4 clean) ──────────────────────────────────
    domain/test_SimulationEngine.cpp
    domain/test_BetheBlochNistRegression.cpp
    domain/test_MaterialRegistry.cpp
    domain/test_ParticleRegistry.cpp
    # ── Platform layer (EventBus, CommandBus, PluginHost) ───────────
    platform/test_EventBus.cpp
    platform/test_CommandBus.cpp
    platform/test_PluginHost.cpp
    # ── Services layer (storage, analysis engines) ──────────────────
    services/test_ProfileManager.cpp
    services/test_QueryCache.cpp
    services/test_JobScheduler.cpp
    services/test_FrameStatisticsPlugin.cpp
    # ── UI shell (no Qt dependency) ────────────────────────────────
    ui/test_ActionRegistry.cpp
    ${PROJECT_SOURCE_DIR}/src/ui/qt/shell/ActionRegistry.cpp
    # F1b: biosim stopping-power path validated vs NIST PSTAR. The physics
    # files below are pure C++ (no Qt) so they compile straight into the
    # headless test binary without pulling in beamlab_biosim/Qt.
    test_BiosimStoppingPowerNistRegression.cpp
    ${PROJECT_SOURCE_DIR}/src/biosim/physics/StoppingPowerEngine.cpp
    ${PROJECT_SOURCE_DIR}/src/biosim/physics/EnergyStraggling.cpp
    ${PROJECT_SOURCE_DIR}/src/biosim/physics/ParticleLibrary.cpp
    ${PROJECT_SOURCE_DIR}/src/biosim/materials/BioMaterialLibrary.cpp
)
# test_StoppingPowerEngine.cpp — disabled: hardcodes the engine's own output
# (self-referential, S5 anti-pattern). Superseded by the NIST-fixture test above.
# pending: services/test_AnalysisOrchestrator.cpp — complex mock; full services migration needed
# pending: services/test_ExporterRegistry.cpp — needs full exporter chain
# pending: services/test_ImporterRegistry.cpp — needs services importer chain
# pending: services/test_DockManager.cpp — requires Qt6::Widgets (UI test)

target_include_directories(beamlab_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)

target_compile_definitions(beamlab_tests PRIVATE
    TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
    PROJECT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)

target_link_libraries(beamlab_tests PRIVATE
    GTest::gtest_main
    beamlab_core
    beamlab_domain
    beamlab_platform
    beamlab_services
    nlohmann_json::nlohmann_json
    sqlite3
)

gtest_discover_tests(beamlab_tests PROPERTIES LABELS unit)
