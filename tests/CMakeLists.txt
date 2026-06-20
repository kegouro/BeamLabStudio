add_subdirectory(unit)

if(BEAMLAB_ENABLE_PERFORMANCE_TESTS)
    add_subdirectory(performance)
endif()
