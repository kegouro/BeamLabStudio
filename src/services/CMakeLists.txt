add_library(beamlab_services STATIC
    analysis/AnalysisOrchestrator.cpp
    analysis/JobScheduler.cpp
    analysis/ThreadPool.cpp
    analysis/engines/FrameStatisticsPlugin.cpp
    storage/QueryCache.cpp
    storage/StorageManager.cpp
    storage/SqliteBackend.cpp
    export/CsvExporter.cpp
    export/ObjExporter.cpp
    export/ExporterRegistry.cpp
    import/HeaderAnalyzer.cpp
    import/ImporterRegistry.cpp
    import/DelimitedTableImporter.cpp
    import/ComsolCsvImporter.cpp
    import/Geant4CsvImporter.cpp
)

target_include_directories(beamlab_services PUBLIC
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(beamlab_services PUBLIC
    beamlab_core
    beamlab_domain
    beamlab_platform
    nlohmann_json::nlohmann_json
    sqlite3
)

if(UNIX)
    target_link_libraries(beamlab_services PRIVATE ${CMAKE_DL_LIBS})
endif()

target_compile_features(beamlab_services PRIVATE cxx_std_20)

# F5: per-target -Werror once services layer is clean.
# ponytail: services warnings not audited; Werror deferred | techo: after F6 migration | upgrade: audit + fix then add -Werror
