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
    domain/test_SimulationEngine.cpp
    domain/test_BetheBlochNistRegression.cpp
    services/test_ProfileManager.cpp
    ui/test_ActionRegistry.cpp
    ${PROJECT_SOURCE_DIR}/src/ui/qt/shell/ActionRegistry.cpp
)
# test_StoppingPowerEngine.cpp — requires old BioMaterialLibrary/ParticleLibrary (pending migration to domain layer)

target_include_directories(beamlab_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)

target_compile_definitions(beamlab_tests PRIVATE
    TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
    PROJECT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)

target_link_libraries(beamlab_tests PRIVATE
    GTest::gtest_main
    beamlab_core
    beamlab_domain
    nlohmann_json::nlohmann_json
    sqlite3
)

gtest_discover_tests(beamlab_tests PROPERTIES LABELS unit)
