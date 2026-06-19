#include "services/import/Geant4CsvImporter.h"
#include "services/import/HeaderAnalyzer.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;
using namespace beamlab::services::import;

// ── Helpers ────────────────────────────────────────────────────────

static std::string writeTempCsv(const std::string& content)
{
    auto path = (fs::temp_directory_path() /
        ("beamlab_g4t_" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()) + ".csv")).string();
    std::ofstream out(path);
    out << content;
    out.close();
    return path;
}

// Minimal storage for test verification.
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

// ═══════════════════════════════════════════════════════════════════
//  Real header from muon_tracks.csv (11 columns, LF endings)
// ═══════════════════════════════════════════════════════════════════

TEST(Geant4CsvImporterPermissive, MuonTracksHeaderScoresHigh)
{
    auto csv = writeTempCsv(
        "x_cm,y_cm,z_cm,edep_MeV,kinE_MeV,momx_MeV,momy_MeV,momz_MeV,time_ns,trackID,eventID\n"
        "0.14,1.06,1155.05,0.01,3000.0,-20.9,-155.4,3099.8,42.5,1,0\n");

    Geant4CsvImporter imp;
    auto score = imp.probe(csv);
    EXPECT_GE(score.value, 0.70);
    fs::remove(csv);
}

TEST(Geant4CsvImporterPermissive, MuonTracks11ColumnImport)
{
    auto csv = writeTempCsv(
        "x_cm,y_cm,z_cm,edep_MeV,kinE_MeV,momx_MeV,momy_MeV,momz_MeV,time_ns,trackID,eventID\n"
        "0.14,1.06,1155.05,0.010,3000.0,-20.9,-155.4,3099.8,42.5,1,0\n"
        "0.08,0.63,1163.55,14.135,2984.9,-16.9,-149.9,3085.1,42.8,1,0\n");

    TestStorage storage;
    Geant4CsvImporter imp;
    imp.import(csv, storage, nullptr);

    ASSERT_EQ(storage.samples.size(), 2u);
    EXPECT_NEAR(storage.samples[0].position_m.x, 0.14 / 100.0, 1e-12);
    EXPECT_NEAR(storage.samples[0].position_m.z, 1155.05 / 100.0, 1e-12);
    EXPECT_NEAR(storage.samples[0].edep_MeV, 0.010, 1e-12);
    // momentum columns: should be 0 since mapping is not used in the radical fallback.
    // (The radical fallback uses idx mapping for required cols only.)
    EXPECT_EQ(storage.samples[1].trajectory_id.value(), 1u);
    fs::remove(csv);
}

// ═══════════════════════════════════════════════════════════════════
//  Real header from energy_deposition_steps(1).csv (15 columns, LF)
// ═══════════════════════════════════════════════════════════════════

TEST(Geant4CsvImporterPermissive, EnergySteps15ColumnImport)
{
    auto csv = writeTempCsv(
        "x_cm,y_cm,z_cm,edep_MeV,kinE_MeV,momx_MeV,momy_MeV,momz_MeV,"
        "time_ns,trackID,parentID,eventID,pdg,particleName,source_file\n"
        "-2.375,0.605,1164.28,0.020,1999.9,2092.5,-135.5,159.7,43.4,2,0,0,-13,mu+,step.root\n"
        "-2.365,0.604,1164.28,0.012,1999.9,2092.5,-135.3,159.7,43.4,2,0,0,-13,mu+,step.root\n");

    TestStorage storage;
    Geant4CsvImporter imp;
    imp.import(csv, storage, nullptr);

    EXPECT_EQ(storage.samples.size(), 2u);
    EXPECT_NEAR(storage.samples[0].edep_MeV, 0.020, 1e-12);
    fs::remove(csv);
}

// ═══════════════════════════════════════════════════════════════════
//  No header — treat first line as data
// ═══════════════════════════════════════════════════════════════════

TEST(Geant4CsvImporterPermissive, NoHeaderNumericOnly)
{
    auto csv = writeTempCsv(
        "0.5,1.0,100.0,0.02\n"
        "0.6,1.5,200.0,0.03\n");

    TestStorage storage;
    Geant4CsvImporter imp;
    imp.import(csv, storage, nullptr);

    EXPECT_EQ(storage.samples.size(), 2u);
    EXPECT_NEAR(storage.samples[0].position_m.x, 0.5 / 100.0, 1e-9);
    fs::remove(csv);
}

// ═══════════════════════════════════════════════════════════════════
//  Semicolon delimiter (European Excel)
// ═══════════════════════════════════════════════════════════════════

TEST(Geant4CsvImporterPermissive, SemicolonDelimiter)
{
    auto csv = writeTempCsv(
        "x_cm;y_cm;z_cm;edep_MeV\n"
        "0.5;1.0;100.0;0.02\n"
        "0.6;1.5;200.0;0.03\n");

    TestStorage storage;
    Geant4CsvImporter imp;
    imp.import(csv, storage, nullptr);

    EXPECT_EQ(storage.samples.size(), 2u);
    fs::remove(csv);
}

// ═══════════════════════════════════════════════════════════════════
//  Tab delimiter
// ═══════════════════════════════════════════════════════════════════

TEST(Geant4CsvImporterPermissive, TabDelimiter)
{
    auto csv = writeTempCsv(
        "x_cm\ty_cm\tz_cm\tedep_MeV\n"
        "0.5\t1.0\t100.0\t0.02\n");

    TestStorage storage;
    Geant4CsvImporter imp;
    imp.import(csv, storage, nullptr);

    EXPECT_EQ(storage.samples.size(), 1u);
    fs::remove(csv);
}

// ═══════════════════════════════════════════════════════════════════
//  Windows CRLF
// ═══════════════════════════════════════════════════════════════════
TEST(Geant4CsvImporterPermissive, WindowsCRLF)
{
    auto path = (fs::temp_directory_path() /
        ("beamlab_crlf_test_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".csv")).string();
    {
        std::ofstream out(path, std::ios::binary);
        out << "x_cm,y_cm,z_cm,edep_MeV\r\n"
               "0.5,1.0,100.0,0.02\r\n";
    }

    TestStorage storage;
    Geant4CsvImporter imp;
    imp.import(path, storage, nullptr);

    EXPECT_EQ(storage.samples.size(), 1u);
    fs::remove(path);
}

// ═══════════════════════════════════════════════════════════════════
//  Spaces after commas
// ═══════════════════════════════════════════════════════════════════

TEST(Geant4CsvImporterPermissive, SpacesAfterCommas)
{
    auto csv = writeTempCsv(
        "x_cm,  y_cm,  z_cm,  edep_MeV\n"
        "0.5,   1.0,   100.0, 0.02\n");

    TestStorage storage;
    Geant4CsvImporter imp;
    imp.import(csv, storage, nullptr);

    EXPECT_EQ(storage.samples.size(), 1u);
    EXPECT_NEAR(storage.samples[0].position_m.x, 0.5 / 100.0, 1e-9);
    fs::remove(csv);
}
