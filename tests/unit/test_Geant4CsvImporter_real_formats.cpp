#include "services/import/Geant4CsvImporter.h"
#include "services/import/HeaderAnalyzer.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;
using namespace beamlab::services::import;

// ── Helpers ────────────────────────────────────────────────────────

static std::string writeTempFile(const std::string& content,
                                  const std::string& suffix = ".csv")
{
    auto path = (fs::temp_directory_path() /
                 ("beamlab_geant4_test_" +
                  std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
                  suffix)).string();
    std::ofstream out(path);
    out << content;
    out.close();
    return path;
}

static void cleanupFile(const std::string& path)
{
    std::error_code ec;
    fs::remove(path, ec);
}

// ═══════════════════════════════════════════════════════════════════
//  Real Format 1 — muon_tracks.csv (11 columns)
//  Observed header:
//    x_cm,y_cm,z_cm,edep_MeV,kinE_MeV,momx_MeV,momy_MeV,momz_MeV,
//    time_ns,trackID,eventID
// ═══════════════════════════════════════════════════════════════════

constexpr const char* kMuonTracksHeader =
    "x_cm,y_cm,z_cm,edep_MeV,kinE_MeV,momx_MeV,momy_MeV,momz_MeV,"
    "time_ns,trackID,eventID";

TEST(Geant4CsvImporter, ProbeMuonTracks11Column)
{
    auto csvPath = writeTempFile(
        std::string(kMuonTracksHeader) + "\n"
        "0.142,1.06,1155.05,0.000,3000.0,-20.9,-155.4,3099.8,42.5,1,0\n"
        "0.142,1.05,1155.11,0.082,2999.4,-21.7,-154.5,3099.3,42.5,1,0\n");

    Geant4CsvImporter importer;
    auto score = importer.probe(csvPath);

    EXPECT_GE(score.value, 0.90)
        << "11-column Geant4 CSV should score ≥0.90. Got: " << score.value;

    cleanupFile(csvPath);
}

TEST(Geant4CsvImporter, ImportMuonTracks11Column)
{
    auto csvPath = writeTempFile(
        std::string(kMuonTracksHeader) + "\n"
        "0.142,1.06,1155.05,0.010,3000.0,-20.9,-155.4,3099.8,42.5,1,0\n"
        "0.142,1.05,1155.11,0.082,2999.4,-21.7,-154.5,3099.3,42.5,1,0\n");

    Geant4CsvImporter importer;

    // Use a minimal in-memory backend.
    class TestStorage : public storage::IStorageBackend {
    public:
        std::vector<beamlab::data::TrajectorySample> samples;
        std::string backendName() const override { return "Test"; }
        uint64_t totalSamples() const override { return samples.size(); }
        uint64_t totalTrajectories() const override { return 1; }
        void beginWriteBatch() override {}
        void writeSample(const beamlab::data::TrajectorySample& s) override {
            samples.push_back(s);
        }
        void endWriteBatch() override {}
        storage::SampleBatch readBatch(uint64_t, uint64_t) const override { return {}; }
        storage::SampleBatch readAxialRange(double, double) const override { return {}; }
        storage::SampleBatch readTrajectory(beamlab::data::TrajectoryId) const override { return {}; }
        storage::GlobalStats globalStats() const override { return {}; }
        void vacuum() override {}
        uint64_t diskUsage() const override { return 0; }
    };

    TestStorage storage;
    importer.import(csvPath, storage, nullptr);

    EXPECT_EQ(storage.samples.size(), 2u);
    EXPECT_NEAR(storage.samples[0].position_m.x, 0.142 / 100.0, 1e-12);
    EXPECT_NEAR(storage.samples[0].position_m.z, 1155.05 / 100.0, 1e-12);
    EXPECT_NEAR(storage.samples[0].edep_MeV, 0.010, 1e-9);
    EXPECT_NEAR(storage.samples[0].kinE_MeV, 3000.0, 1e-9);

    cleanupFile(csvPath);
}

// ═══════════════════════════════════════════════════════════════════
//  Real Format 2 — energy_deposition_steps(1).csv (15 columns)
//  Observed header:
//    x_cm,y_cm,z_cm,edep_MeV,kinE_MeV,momx_MeV,momy_MeV,momz_MeV,
//    time_ns,trackID,parentID,eventID,pdg,particleName,source_file
//  particleName is QUOTED: mu+
// ═══════════════════════════════════════════════════════════════════

constexpr const char* kEnergyStepsHeader =
    "x_cm,y_cm,z_cm,edep_MeV,kinE_MeV,momx_MeV,momy_MeV,momz_MeV,"
    "time_ns,trackID,parentID,eventID,pdg,particleName,source_file";

TEST(Geant4CsvImporter, ProbeEnergySteps15Column)
{
    auto csvPath = writeTempFile(
        std::string(kEnergyStepsHeader) + "\n"
        "-2.375,0.605,1164.28,0.020,1999.9,2092.5,-135.5,159.7,43.4,2,0,0,-13,\"mu+\",step.root\n");

    Geant4CsvImporter importer;
    auto score = importer.probe(csvPath);

    EXPECT_GE(score.value, 0.90)
        << "15-column Geant4 CSV should score ≥0.90. Got: " << score.value;

    cleanupFile(csvPath);
}

TEST(Geant4CsvImporter, ImportEnergySteps15Column)
{
    auto csvPath = writeTempFile(
        std::string(kEnergyStepsHeader) + "\n"
        "-2.375,0.605,1164.28,0.020,1999.9,2092.5,-135.5,159.7,43.4,2,0,0,-13,\"mu+\",step.root\n"
        "-2.365,0.604,1164.28,0.012,1999.9,2092.5,-135.3,159.7,43.4,2,0,0,-13,\"mu+\",step.root\n");

    Geant4CsvImporter importer;

    class TestStorage : public storage::IStorageBackend {
    public:
        std::vector<beamlab::data::TrajectorySample> samples;
        std::string backendName() const override { return "Test"; }
        uint64_t totalSamples() const override { return samples.size(); }
        uint64_t totalTrajectories() const override { return 1; }
        void beginWriteBatch() override {}
        void writeSample(const beamlab::data::TrajectorySample& s) override {
            samples.push_back(s);
        }
        void endWriteBatch() override {}
        storage::SampleBatch readBatch(uint64_t, uint64_t) const override { return {}; }
        storage::SampleBatch readAxialRange(double, double) const override { return {}; }
        storage::SampleBatch readTrajectory(beamlab::data::TrajectoryId) const override { return {}; }
        storage::GlobalStats globalStats() const override { return {}; }
        void vacuum() override {}
        uint64_t diskUsage() const override { return 0; }
    };

    TestStorage storage;
    importer.import(csvPath, storage, nullptr);

    EXPECT_EQ(storage.samples.size(), 2u);
    EXPECT_NEAR(storage.samples[0].edep_MeV, 0.020, 1e-9);
    EXPECT_NEAR(storage.samples[0].trajectory_id.value(), 2u);

    cleanupFile(csvPath);
}

// ═══════════════════════════════════════════════════════════════════
//  Variant: Different column order
// ═══════════════════════════════════════════════════════════════════

TEST(Geant4CsvImporter, ProbePermutedColumns)
{
    // z_cm at position 0, x_cm at 1, y_cm at 2.
    auto csvPath = writeTempFile(
        "z_cm,x_cm,y_cm,edep_MeV,kinE_MeV\n"
        "100.0,1.0,2.0,0.01,99.0\n");

    Geant4CsvImporter importer;
    auto score = importer.probe(csvPath);
    EXPECT_GE(score.value, 0.70) << "Permuted columns should score ≥0.70";
    cleanupFile(csvPath);
}

// ═══════════════════════════════════════════════════════════════════
//  Variant: Case-insensitivity + underscores
// ═══════════════════════════════════════════════════════════════════

TEST(Geant4CsvImporter, ProbeCaseInsensitiveHeaders)
{
    auto csvPath = writeTempFile(
        "X_CM,Y_cm,Z_cm,EDEP_MEV,KINE_MEV\n"
        "1.0,2.0,3.0,0.01,100.0\n");

    Geant4CsvImporter importer;
    auto score = importer.probe(csvPath);
    EXPECT_GE(score.value, 0.70) << "Mixed-case headers should score ≥0.70";
    cleanupFile(csvPath);
}

TEST(Geant4CsvImporter, ProbeAlternativeHeaderNames)
{
    auto csvPath = writeTempFile(
        "x[cm],y[cm],z[cm],edep[MeV],Kinetic_Energy,track_id\n"
        "1.0,2.0,3.0,0.01,100.0,42\n");

    Geant4CsvImporter importer;
    auto score = importer.probe(csvPath);
    EXPECT_GE(score.value, 0.70)
        << "Alternative header names should score ≥0.70. Got: " << score.value;
    cleanupFile(csvPath);
}

// ═══════════════════════════════════════════════════════════════════
//  Edge cases
// ═══════════════════════════════════════════════════════════════════

TEST(Geant4CsvImporter, ProbeNonGeant4CsvReturnsZero)
{
    auto csvPath = writeTempFile(
        "name,age,city\n"
        "Alice,30,London\n");

    Geant4CsvImporter importer;
    auto score = importer.probe(csvPath);
    EXPECT_NEAR(score.value, 0.0, 0.01);
    cleanupFile(csvPath);
}

TEST(Geant4CsvImporter, ProbeEmptyFileReturnsZero)
{
    auto csvPath = writeTempFile("");

    Geant4CsvImporter importer;
    auto score = importer.probe(csvPath);
    EXPECT_NEAR(score.value, 0.0, 0.01);
    cleanupFile(csvPath);
}

TEST(Geant4CsvImporter, ImportMissingRequiredColumnThrows)
{
    auto csvPath = writeTempFile(
        "x_cm,y_cm,kinE_MeV\n"  // Missing z_cm and edep_MeV
        "1.0,2.0,100.0\n");

    Geant4CsvImporter importer;

    class TestStorage : public storage::IStorageBackend {
    public:
        std::string backendName() const override { return "Test"; }
        uint64_t totalSamples() const override { return 0; }
        uint64_t totalTrajectories() const override { return 0; }
        void beginWriteBatch() override {}
        void writeSample(const beamlab::data::TrajectorySample&) override {}
        void endWriteBatch() override {}
        storage::SampleBatch readBatch(uint64_t, uint64_t) const override { return {}; }
        storage::SampleBatch readAxialRange(double, double) const override { return {}; }
        storage::SampleBatch readTrajectory(beamlab::data::TrajectoryId) const override { return {}; }
        storage::GlobalStats globalStats() const override { return {}; }
        void vacuum() override {}
        uint64_t diskUsage() const override { return 0; }
    };

    TestStorage storage;
    EXPECT_THROW(importer.import(csvPath, storage, nullptr),
                 std::runtime_error);
    cleanupFile(csvPath);
}

TEST(Geant4CsvImporter, MalformedRowSkipped)
{
    auto csvPath = writeTempFile(
        std::string(kMuonTracksHeader) + "\n"
        "garbage,text,not_a_number,here,skip,this,line,entirely,today,please,thanks\n"
        "0.142,1.06,1155.05,0.010,3000.0,-20.9,-155.4,3099.8,42.5,1,0\n");

    Geant4CsvImporter importer;

    class TestStorage : public storage::IStorageBackend {
    public:
        std::vector<beamlab::data::TrajectorySample> samples;
        std::string backendName() const override { return "Test"; }
        uint64_t totalSamples() const override { return samples.size(); }
        uint64_t totalTrajectories() const override { return 1; }
        void beginWriteBatch() override {}
        void writeSample(const beamlab::data::TrajectorySample& s) override {
            samples.push_back(s);
        }
        void endWriteBatch() override {}
        storage::SampleBatch readBatch(uint64_t, uint64_t) const override { return {}; }
        storage::SampleBatch readAxialRange(double, double) const override { return {}; }
        storage::SampleBatch readTrajectory(beamlab::data::TrajectoryId) const override { return {}; }
        storage::GlobalStats globalStats() const override { return {}; }
        void vacuum() override {}
        uint64_t diskUsage() const override { return 0; }
    };

    TestStorage storage;
    importer.import(csvPath, storage, nullptr);

    // Malformed row should be skipped, not crash. One valid row imported.
    EXPECT_EQ(storage.samples.size(), 1u);
    cleanupFile(csvPath);
}
