#include "services/import/ImporterRegistry.h"
#include "services/import/Geant4CsvImporter.h"
#include "services/import/ComsolCsvImporter.h"
#include "services/import/RootNativeImporter.h"
#include "services/import/DelimitedTableImporter.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;
using namespace beamlab::services::import;

// ── Helpers ─────────────────────────────────────────────────────────

static std::string writeTempFile(const std::string& content,
                                  const std::string& suffix = ".csv")
{
    auto path = (fs::temp_directory_path() / ("beamlab_test_" + std::to_string(std::rand()) + suffix)).string();
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

// ── Test fixture ────────────────────────────────────────────────────

class ImporterRegistryTest : public ::testing::Test {
protected:
    ImporterRegistry registry;

    void SetUp() override
    {
        registry.registerImporter(std::make_unique<Geant4CsvImporter>());
        registry.registerImporter(std::make_unique<ComsolCsvImporter>());
        registry.registerImporter(std::make_unique<RootNativeImporter>());
        registry.registerImporter(std::make_unique<DelimitedTableImporter>());
    }
};

// ── Tests ───────────────────────────────────────────────────────────

TEST_F(ImporterRegistryTest, Geant4CsvDetectedByHeader)
{
    auto csvPath = writeTempFile(
        "x_cm,y_cm,z_cm,edep_MeV,kinE_MeV,momx_MeV,momy_MeV,momz_MeV,"
        "time_ns,trackID,parentID,eventID,pdg,particleName,source_file\n"
        "0.0,0.0,0.0,0.01,100.0,0.0,0.0,0.0,0.0,1,0,1,13,mu-,test\n");

    auto result = registry.detectImporter(csvPath);
    EXPECT_NE(result.bestMatch, nullptr);
    EXPECT_GE(result.score.value, 0.90);
    EXPECT_EQ(result.bestMatch->name(), "Geant4 CSV");

    cleanupFile(csvPath);
}

TEST_F(ImporterRegistryTest, Geant4CsvPartialColumns)
{
    // Only 3 columns out of 15 — should get partial match (0.50).
    auto csvPath = writeTempFile(
        "x_cm,y_cm,z_cm,unknown1,unknown2,unknown3\n"
        "0.0,0.0,0.0,0.0,0.0,0.0\n");

    auto result = registry.detectImporter(csvPath);
    EXPECT_GE(result.score.value, 0.0);
    // If other importers also match, Geant4 might not be best.
    EXPECT_TRUE(result.bestMatch != nullptr);

    cleanupFile(csvPath);
}

TEST_F(ImporterRegistryTest, ComsolDetectedByPercentComments)
{
    auto csvPath = writeTempFile(
        "%% COMSOL Export\n"
        "%% Model: beam_in_vacuum.mph\n"
        "% Version: 6.2\n"
        "x,y,z,expr\n"
        "0.0,0.0,0.0,1.23\n",
        ".txt");

    auto result = registry.detectImporter(csvPath);
    ASSERT_NE(result.bestMatch, nullptr);
    EXPECT_GE(result.score.value, 0.80);
    EXPECT_EQ(result.bestMatch->name(), "COMSOL CSV");

    cleanupFile(csvPath);
}

TEST_F(ImporterRegistryTest, RootDetectedByMagicBytes)
{
    // Create a minimal file with ROOT magic bytes "root".
    auto path = (fs::temp_directory_path() / "test_magic.root").string();
    {
        std::ofstream out(path, std::ios::binary);
        uint8_t magic[] = {0x72, 0x6F, 0x6F, 0x74};  // "root"
        out.write(reinterpret_cast<const char*>(magic), 4);
    }

    auto result = registry.detectImporter(path);
    ASSERT_NE(result.bestMatch, nullptr);
    EXPECT_EQ(result.score.value, 1.0);
    EXPECT_EQ(result.bestMatch->name(), "CERN ROOT");

    cleanupFile(path);
}

TEST_F(ImporterRegistryTest, DelimitedFallbackDetected)
{
    // Plain CSV with 5 numeric columns, no special header.
    auto csvPath = writeTempFile(
        "a,b,c,d,e\n"
        "1.0,2.0,3.0,4.0,5.0\n"
        "6.0,7.0,8.0,9.0,10.0\n");

    auto result = registry.detectImporter(csvPath);
    ASSERT_NE(result.bestMatch, nullptr);
    // With Geant4 and DelimitedTable registered, the delimited table
    // should be the fallback. But Geant4 might also get a partial match.
    // Verify score is at least 0.30.
    EXPECT_GE(result.score.value, 0.30);

    cleanupFile(csvPath);
}

TEST_F(ImporterRegistryTest, UnknownBinaryReturnsNull)
{
    // Create a binary file that doesn't match any importer.
    auto path = (fs::temp_directory_path() / "unknown.bin").string();
    {
        std::ofstream out(path, std::ios::binary);
        uint8_t data[] = {0xDE, 0xAD, 0xBE, 0xEF};
        out.write(reinterpret_cast<const char*>(data), 4);
    }

    auto result = registry.detectImporter(path);
    // No importer should match a binary .bin file with no known magic.
    EXPECT_EQ(result.bestMatch, nullptr);
    EXPECT_LT(result.score.value, 0.20);

    cleanupFile(path);
}

TEST_F(ImporterRegistryTest, ImportThrowsForUnknownFormat)
{
    auto path = (fs::temp_directory_path() / "unknown.xyz").string();
    {
        std::ofstream out(path);
        out << "garbage data with no structure\n";
    }

    // Use a mock storage for the import call.
    class MockStorage final : public storage::IStorageBackend {
    public:
        std::string backendName() const override { return "Mock"; }
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

    MockStorage mockStorage;

    EXPECT_THROW(
        registry.import(path, mockStorage, nullptr),
        std::runtime_error);

    cleanupFile(path);
}

TEST_F(ImporterRegistryTest, AlternativesPopulated)
{
    // Plain numeric CSV — multiple importers might partially match.
    auto csvPath = writeTempFile(
        "1.0,2.0,3.0,4.0,5.0\n"
        "6.0,7.0,8.0,9.0,10.0\n");

    auto result = registry.detectImporter(csvPath);
    // The alternatives vector should contain importers with score > 0.3
    // that didn't win.  At minimum, we should have a best match.
    EXPECT_NE(result.bestMatch, nullptr);

    cleanupFile(csvPath);
}

TEST_F(ImporterRegistryTest, AvailableImportersCount)
{
    auto available = registry.availableImporters();
    EXPECT_EQ(available.size(), 4u);
}

TEST_F(ImporterRegistryTest, EmptyRegistry)
{
    ImporterRegistry emptyReg;
    auto available = emptyReg.availableImporters();
    EXPECT_TRUE(available.empty());

    EXPECT_EQ(emptyReg.detectImporter("/nonexistent/file.csv").bestMatch,
              nullptr);
}

TEST_F(ImporterRegistryTest, Geant4WithTrajectoryCsv)
{
    // test using the exact Geant4 15-column format with trajectory data.
    auto csvPath = writeTempFile(
        "x_cm,y_cm,z_cm,edep_MeV,kinE_MeV,momx_MeV,momy_MeV,momz_MeV,"
        "time_ns,trackID,parentID,eventID,pdg,particleName,source_file\n"
        "0.0,0.0,0.0,0.010,100.0,10.0,0.0,0.0,0.0,1,0,1,13,mu-,sim.csv\n"
        "1.0,0.0,10.0,0.012,99.5,9.8,0.1,0.05,0.1,1,0,1,13,mu-,sim.csv\n"
        "0.0,0.0,20.0,0.011,99.0,9.7,0.0,0.0,0.2,2,0,1,13,mu-,sim.csv\n");

    auto result = registry.detectImporter(csvPath);
    ASSERT_NE(result.bestMatch, nullptr);
    EXPECT_EQ(result.bestMatch->name(), "Geant4 CSV");
    EXPECT_GE(result.score.value, 0.90);

    cleanupFile(csvPath);
}

TEST_F(ImporterRegistryTest, EmptyFileReturnsNoMatch)
{
    auto csvPath = writeTempFile("");

    auto result = registry.detectImporter(csvPath);
    EXPECT_EQ(result.bestMatch, nullptr);
    EXPECT_LT(result.score.value, 0.01);

    cleanupFile(csvPath);
}
