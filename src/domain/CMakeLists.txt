add_library(beamlab_domain STATIC
    physics/ParticleRegistry.cpp
    materials/MaterialRegistry.cpp
    simulation/SimulationEngine.cpp
)

target_include_directories(beamlab_domain PUBLIC
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(beamlab_domain PUBLIC
    beamlab_core
    nlohmann_json::nlohmann_json
)

target_compile_features(beamlab_domain PRIVATE cxx_std_20)

# F5: zero warnings confirmed; promote to errors so they don't regress.
target_compile_options(beamlab_domain PRIVATE -Werror)
