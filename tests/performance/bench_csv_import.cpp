#include "services/storage/SqliteBackend.h"
#include "services/storage/StorageManager.h"
#include "services/import/Geant4CsvImporter.h"
#include "core/config/AnalysisConfig.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <chrono>
#include <iostream>

namespace fs = std::filesystem;
using namespace beamlab::services::storage;
using namespace beamlab::services::import;
using namespace beamlab::core;

// ── Test fixture ────────────────────────────────────────────────────

class CsvImportBenchmark : public ::testing::Test {
protected:
    fs::path tmpCsv;

    void TearDown() override
    {
        if (!tmpCsv.empty() && fs::exists(tmpCsv))
            fs::remove(tmpCsv);
    }

    std::string generateSyntheticCSV(uint64_t targetBytes)
    {
        auto path = fs::temp_directory_path() / "beamlab_bench_import.csv";
        tmpCsv = path;

        std::ofstream out(path);
        // Geant4 15-column header.
        out << "x_cm,y_cm,z_cm,edep_MeV,kinE_MeV,"
               "momx_MeV,momy_MeV,momz_MeV,time_ns,"
               "trackID,parentID,eventID,pdg,particleName,source_file\n";

        uint64_t written = out.tellp();
        int sampleIdx = 0;
        while (written < targetBytes) {
            out << (sampleIdx * 0.01) << ","    // x_cm
                << (sampleIdx * 0.005) << ","   // y_cm
                << (sampleIdx * 0.02) << ","    // z_cm
                << "0.010,"                     // edep
                << "100.0,"                     // kinE
                << "10.0,0.0,0.0,"             // momentum
                << (sampleIdx * 0.1) << ","     // time_ns
                << (sampleIdx % 10 + 1) << ","   // trackID
                << "0,1,13,mu-,simulated\n";
            ++sampleIdx;
            written = static_cast<uint64_t>(out.tellp());
        }
        out.close();
        return path.string();
    }
};

// ── Benchmarks ─────────────────────────────────────────────────────

TEST_F(CsvImportBenchmark, Import_10MB_CSV)
{
    auto csvPath = generateSyntheticCSV(10 * 1024 * 1024);  // 10 MB

    AnalysisRunConfig config;
    config.storage.sqlite_threshold_mb = 10;
    config.storage.batch_size = 50000;

    StorageManager mgr;
    auto backend = mgr.create(fs::file_size(csvPath), config);

    Geant4CsvImporter importer;

    auto start = std::chrono::high_resolution_clock::now();
    importer.import(csvPath, *backend, nullptr);
    auto elapsed = std::chrono::high_resolution_clock::now() - start;

    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    auto samples = backend->totalSamples();

    std::cout << "[Benchmark] Import 10 MB CSV:\n"
              << "  Time:   " << ms << " ms\n"
              << "  Samples: " << samples << "\n"
              << "  Rate:   " << (samples * 1000.0 / (ms > 0 ? ms : 1))
              << " samples/s\n";

    EXPECT_GT(samples, 0u);
    EXPECT_LT(ms, 5000) << "10 MB import took " << ms << "ms (target: <5s)";
}

TEST_F(CsvImportBenchmark, Import_Empty_CSV)
{
    auto csvPath = generateSyntheticCSV(1024);  // Just header + a few rows.

    AnalysisRunConfig config;
    config.storage.sqlite_threshold_mb = 1;
    StorageManager mgr;
    auto backend = mgr.create(fs::file_size(csvPath), config);

    Geant4CsvImporter importer;
    auto start = std::chrono::high_resolution_clock::now();
    importer.import(csvPath, *backend, nullptr);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - start).count();

    std::cout << "[Benchmark] Import tiny CSV: " << ms << " ms\n";
    EXPECT_LT(ms, 1000) << "Tiny CSV import took " << ms << "ms (target: <1s)";
}
