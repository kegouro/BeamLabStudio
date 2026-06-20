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
    domain/test_SimulationEngine.cpp
    domain/test_BetheBlochNistRegression.cpp
    services/test_ProfileManager.cpp
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
