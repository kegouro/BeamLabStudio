FetchContent_MakeAvailable(googletest)

include(GoogleTest)

add_executable(beamlab_tests
    test_FrameStatisticsEngine.cpp
    test_FocusDetector.cpp
    test_Geant4CsvImporter.cpp
    test_StoppingPowerEngine.cpp
    test_SurfaceBuilder.cpp
)

target_include_directories(beamlab_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)

target_compile_definitions(beamlab_tests PRIVATE TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")

target_link_libraries(beamlab_tests PRIVATE
    GTest::gtest_main
    beamlab_core
    beamlab_biosim
)

gtest_discover_tests(beamlab_tests)
