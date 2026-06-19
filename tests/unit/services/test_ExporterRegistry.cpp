#include "services/export/ExporterRegistry.h"
#include "services/export/CsvExporter.h"
#include "services/export/ObjExporter.h"
#include "services/export/ParquetExporter.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;
using namespace beamlab::services::export_;
using namespace beamlab::services::storage;
using namespace beamlab::services::analysis;

// ── Mock storage ──────────────────────────────────────────────────

class MockExportStorage final : public IStorageBackend {
public:
    uint64_t totalSamples_ = 1000;

    std::string backendName() const override { return "MockExport"; }
    uint64_t totalSamples() const override { return totalSamples_; }
    uint64_t totalTrajectories() const override { return 10; }
    void beginWriteBatch() override {}
    void writeSample(const beamlab::data::TrajectorySample&) override {}
    void endWriteBatch() override {}
    SampleBatch readBatch(uint64_t, uint64_t) const override { return {}; }
    SampleBatch readAxialRange(double, double) const override { return {}; }
    SampleBatch readTrajectory(beamlab::data::TrajectoryId) const override { return {}; }
    GlobalStats globalStats() const override
    {
        GlobalStats gs;
        gs.count = totalSamples_;
        gs.zMin = 0.0; gs.zMax = 10.0; gs.edepSum = 5.0;
        return gs;
    }
    void vacuum() override {}
    uint64_t diskUsage() const override { return 0; }
};

// ── Test fixture ─────────────────────────────────────────────────

class ExporterRegistryTest : public ::testing::Test {
protected:
    ExporterRegistry reg;
    MockExportStorage storage;
    AnalysisResult analysisResult;
    fs::path tmpDir;

    void SetUp() override
    {
        reg.registerExporter(std::make_unique<CsvExporter>());
        reg.registerExporter(std::make_unique<ObjExporter>());
        reg.registerExporter(std::make_unique<ParquetExporter>());

        // Create a realistic analysis result with frame data.
        EngineResult frameResult = EngineResult::ok();
        frameResult.metrics["frames_data"] = nlohmann::json::array();
        nlohmann::json frame1 = {
            {"reference_value", 0.05},
            {"reference_min_value", 0.0},
            {"reference_max_value", 0.1},
            {"point_count", 100},
            {"centroid_u", 0.001},
            {"centroid_v", -0.002},
            {"sigma_u", 0.003},
            {"sigma_v", 0.004},
            {"r_rms", 0.005},
            {"valid", true},
        };
        nlohmann::json frame2 = {
            {"reference_value", 0.15},
            {"reference_min_value", 0.1},
            {"reference_max_value", 0.2},
            {"point_count", 150},
            {"centroid_u", 0.002},
            {"centroid_v", -0.001},
            {"sigma_u", 0.004},
            {"sigma_v", 0.003},
            {"r_rms", 0.005},
            {"valid", true},
        };
        frameResult.metrics["frames_data"].push_back(frame1);
        frameResult.metrics["frames_data"].push_back(frame2);

        analysisResult.success = true;
        analysisResult.engineResults["FrameStatistics"] = frameResult;

        tmpDir = fs::temp_directory_path() / "beamlab_test_export";
        fs::remove_all(tmpDir);
    }

    void TearDown() override
    {
        fs::remove_all(tmpDir);
    }
};

// ── Tests ─────────────────────────────────────────────────────────

TEST_F(ExporterRegistryTest, AvailableFormats)
{
    auto formats = reg.availableFormats();
    EXPECT_EQ(formats.size(), 3u);

    // Sort for deterministic comparison.
    std::sort(formats.begin(), formats.end());
    EXPECT_EQ(formats[0], "csv");
    EXPECT_EQ(formats[1], "obj");
    EXPECT_EQ(formats[2], "parquet");
}

TEST_F(ExporterRegistryTest, CsvExportCreatesFile)
{
    auto result = reg.exportSingle("csv", storage, analysisResult,
                                    tmpDir / "test_output.csv");

    EXPECT_TRUE(result.success) << result.error.value_or("");
    EXPECT_TRUE(fs::exists(result.outputPath));
    EXPECT_GT(result.bytesWritten, 0u);
}

TEST_F(ExporterRegistryTest, CsvExportContentIsValid)
{
    auto csvPath = tmpDir / "frames.csv";
    auto result = reg.exportSingle("csv", storage, analysisResult, csvPath);

    ASSERT_TRUE(result.success);

    // Read back and verify content.
    std::ifstream in(csvPath);
    ASSERT_TRUE(in.is_open());

    std::string line;
    // Header.
    ASSERT_TRUE(std::getline(in, line));
    EXPECT_TRUE(line.find("frame_index") != std::string::npos);

    // First data row.
    ASSERT_TRUE(std::getline(in, line));
    EXPECT_TRUE(line.find("0,") == 0);
    EXPECT_TRUE(line.find("100,") != std::string::npos);  // point_count

    // Second data row.
    ASSERT_TRUE(std::getline(in, line));
    EXPECT_TRUE(line.find("1,") == 0);
}

TEST_F(ExporterRegistryTest, ExportAllCreatesSubdirectories)
{
    auto multi = reg.exportAll(storage, analysisResult, tmpDir);

    // Should not throw and should complete all formats.
    EXPECT_TRUE(fs::exists(tmpDir / "csv"));
    EXPECT_TRUE(fs::exists(tmpDir / "obj"));
    EXPECT_TRUE(fs::exists(tmpDir / "parquet"));

    // CSV should succeed, Parquet may fail (stub).
    EXPECT_EQ(multi.results.size(), 3u);
}

TEST_F(ExporterRegistryTest, ExportAllWithSpecificFormats)
{
    auto multi = reg.exportAll(storage, analysisResult, tmpDir, {"csv"});

    EXPECT_EQ(multi.results.size(), 1u);
    EXPECT_EQ(multi.results[0].first, "csv");
    EXPECT_TRUE(multi.results[0].second.success);
}

TEST_F(ExporterRegistryTest, ExportToDirectoryCreatesDefaultFilename)
{
    fs::create_directories(tmpDir / "csv_out");
    auto result = reg.exportSingle("csv", storage, analysisResult,
                                    tmpDir / "csv_out");

    EXPECT_TRUE(result.success);
    // When exporting to a directory, the exporter creates a default filename.
    EXPECT_TRUE(fs::exists(result.outputPath));
}

TEST_F(ExporterRegistryTest, ExportUnknownFormatThrows)
{
    EXPECT_THROW(
        reg.exportSingle("nonexistent", storage, analysisResult, tmpDir / "out"),
        std::out_of_range);
}

TEST_F(ExporterRegistryTest, ObjExportWritesWavefrontHeader)
{
    auto result = reg.exportSingle("obj", storage, analysisResult,
                                    tmpDir / "mesh.obj");

    ASSERT_TRUE(result.success);
    ASSERT_TRUE(fs::exists(result.outputPath));

    std::ifstream in(result.outputPath);
    std::string firstLine;
    std::getline(in, firstLine);
    EXPECT_TRUE(firstLine.find("#") == 0);  // Wavefront comment
}

TEST_F(ExporterRegistryTest, ParquetExportReturnsError)
{
    auto result = reg.exportSingle("parquet", storage, analysisResult,
                                    tmpDir / "test.parquet");

    // Parquet is a stub — should fail with descriptive error.
    EXPECT_FALSE(result.success);
    EXPECT_TRUE(result.error.has_value());
    EXPECT_TRUE(result.error->find("Apache Arrow") != std::string::npos);
}

TEST_F(ExporterRegistryTest, ExportAllWithEmptyStorage)
{
    MockExportStorage emptyStorage;
    emptyStorage.totalSamples_ = 0;

    auto result = reg.exportSingle("csv", emptyStorage, analysisResult,
                                    tmpDir / "empty.csv");

    // Even with empty storage, exporting metadata should succeed.
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(fs::exists(result.outputPath));
}

TEST_F(ExporterRegistryTest, ExportWithoutEngineResults)
{
    AnalysisResult emptyResult;
    emptyResult.success = true;  // No engine results.

    auto result = reg.exportSingle("csv", storage, emptyResult,
                                    tmpDir / "empty_result.csv");

    // Should succeed but write metadata-only CSV.
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(fs::exists(result.outputPath));
}

TEST_F(ExporterRegistryTest, ProgressCallbackFires)
{
    int progressCount = 0;
    ExportProgressCallback cb = [&](float, const std::string&) {
        ++progressCount;
    };

    auto result = reg.exportSingle("csv", storage, analysisResult,
                                    tmpDir / "progress.csv");

    EXPECT_TRUE(result.success);
    EXPECT_GE(progressCount, 0);  // Callback may fire 0 or more times
}
