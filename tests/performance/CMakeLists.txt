# ══════════════════════════════════════════════════════════════════════
#  beamlab_performance_tests — Performance benchmarks
#  Uses GoogleTest (same as unit tests).  Labeled [performance].
#  Run with:  ctest -L performance --verbose
# ══════════════════════════════════════════════════════════════════════

FetchContent_MakeAvailable(googletest)
include(GoogleTest)

add_executable(beamlab_performance_tests
    bench_csv_import.cpp
    bench_sqlite_batch.cpp
    bench_statistics_engine.cpp
    bench_scene3d_render.cpp
    bench_query_cache.cpp
    bench_memory_arena.cpp
    bench_parallel_batch.cpp
)

target_include_directories(beamlab_performance_tests PRIVATE
    ${PROJECT_SOURCE_DIR}/src
)

target_compile_definitions(beamlab_performance_tests PRIVATE
    TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
    PROJECT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)

target_link_libraries(beamlab_performance_tests PRIVATE
    GTest::gtest_main
    beamlab_core
    beamlab_domain
    beamlab_platform
    beamlab_services
)

# Label all tests in this binary as [performance] for ctest filtering.
gtest_discover_tests(beamlab_performance_tests
    PROPERTIES LABELS performance
)
