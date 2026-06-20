add_library(beamlab_platform STATIC
    EventBus.cpp
    CommandBus.cpp
    PluginHost.cpp
    ServiceRegistry.cpp
)

target_include_directories(beamlab_platform PUBLIC
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(beamlab_platform PUBLIC
    beamlab_core
    nlohmann_json::nlohmann_json
)

if(UNIX)
    target_link_libraries(beamlab_platform PRIVATE ${CMAKE_DL_LIBS})
endif()

target_compile_features(beamlab_platform PRIVATE cxx_std_20)

# F5: per-target -Werror once platform layer is clean.
# ponytail: platform warnings not audited; Werror deferred | techo: after F6 | upgrade: audit + fix
