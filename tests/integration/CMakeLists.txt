# ══════════════════════════════════════════════════════════════════════
#  beamlab_integration_tests — End-to-end integration tests
#  Run with:  ctest -L integration --verbose
# ══════════════════════════════════════════════════════════════════════

FetchContent_MakeAvailable(googletest)
include(GoogleTest)

add_executable(beamlab_integration_tests
)

target_include_directories(beamlab_integration_tests PRIVATE
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/src
)

target_compile_definitions(beamlab_integration_tests PRIVATE
    TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
    PROJECT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)

target_link_libraries(beamlab_integration_tests PRIVATE
    GTest::gtest_main
    beamlab_core
    beamlab_domain
    beamlab_platform
    beamlab_services
    nlohmann_json::nlohmann_json
    sqlite3
)

gtest_discover_tests(beamlab_integration_tests
    PROPERTIES LABELS integration
)
