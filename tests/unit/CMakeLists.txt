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
)
# pending: services/test_AnalysisOrchestrator.cpp — complex mock; EventBus integration needed
# pending: services/test_ExporterRegistry.cpp — needs full exporter chain
# pending: services/test_ImporterRegistry.cpp — needs services importer chain
# pending: services/test_DockManager.cpp — requires Qt6::Widgets (UI test)
# pending: services/test_AnalysisOrchestrator.cpp — pending full services migration
# pending: test_StoppingPowerEngine.cpp — requires old BioMaterialLibrary/ParticleLibrary (pending migration to domain layer)

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
