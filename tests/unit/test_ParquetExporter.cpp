#include "services/export/ParquetExporter.h"
#include "services/storage/IStorageBackend.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;
using namespace beamlab::services::export_;
using namespace beamlab::services::storage;
using namespace beamlab::services::analysis;
using namespace beamlab::data;

// ── Mock storage that provides synthetic samples ────────────────────

class ParquetMockStorage final : public IStorageBackend {
public:
    std::vector<TrajectorySample> samples;

    std::string backendName() const override { return "ParquetMock"; }
    uint64_t totalSamples() const override { return samples.size(); }
    uint64_t totalTrajectories() const override { return 3; }
    void beginWriteBatch() override {}
    void writeSample(const TrajectorySample& s) override { samples.push_back(s); }
    void endWriteBatch() override {}

    SampleBatch readBatch(uint64_t offset, uint64_t count) const override
    {
        if (offset >= samples.size()) return {nullptr, 0, offset};
        auto avail = std::min<uint64_t>(count, samples.size() - offset);
        buf_.assign(samples.begin() + static_cast<ptrdiff_t>(offset),
                    samples.begin() + static_cast<ptrdiff_t>(offset + avail));
        return {buf_.data(), avail, offset};
    }

    SampleBatch readAxialRange(double, double) const override { return {}; }
    SampleBatch readTrajectory(TrajectoryId) const override { return {}; }
    GlobalStats globalStats() const override { return {}; }
    void vacuum() override {}
    uint64_t diskUsage() const override { return 0; }

private:
    mutable std::vector<TrajectorySample> buf_;
};

// ── Fixture ─────────────────────────────────────────────────────────

class ParquetExporterTest : public ::testing::Test {
protected:
    ParquetExporter exporter;
    ParquetMockStorage storage;
    AnalysisResult analysisResult;
    fs::path tmpDir;

    void SetUp() override
    {
        // Populate storage with 5 synthetic samples.
        for (int i = 0; i < 5; ++i) {
            TrajectorySample s;
            s.trajectory_id = TrajectoryId(static_cast<uint64_t>(i % 3 + 1));
            s.position_m = {static_cast<double>(i) * 0.01,
                            static_cast<double>(i) * 0.005,
                            static_cast<double>(i) * 0.1};
            s.edep_MeV = 0.01 + static_cast<double>(i) * 0.001;
            s.kinE_MeV = 100.0 - static_cast<double>(i) * 5.0;
            storage.samples.push_back(s);
        }

        analysisResult.success = true;

        tmpDir = fs::temp_directory_path() / "beamlab_parquet_test";
        fs::create_directories(tmpDir);
    }

    void TearDown() override
    {
        fs::remove_all(tmpDir);
    }

    // Read entire file into string for content verification.
    std::string readFile(const fs::path& path)
    {
        std::ifstream in(path);
        std::stringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }
};

// ── Tests ───────────────────────────────────────────────────────────

TEST_F(ParquetExporterTest, ExportToDirectoryCreatesInterimFile)
{
    auto result = exporter.exportData(storage, analysisResult, tmpDir, nullptr);

    EXPECT_TRUE(result.success) << result.error.value_or("");
    EXPECT_TRUE(fs::exists(result.outputPath));
    EXPECT_GT(result.bytesWritten, 0u);

    // File should have .parquet.csv extension.
    EXPECT_TRUE(result.outputPath.extension() == ".csv");
    EXPECT_TRUE(result.outputPath.stem().string().find(".parquet") != std::string::npos);
}

TEST_F(ParquetExporterTest, ExportToFilePathUsesCorrectName)
{
    auto path = tmpDir / "my_export.parquet";
    auto result = exporter.exportData(storage, analysisResult, path, nullptr);

    EXPECT_TRUE(result.success);
    // The actual output should be my_export.parquet.csv
    EXPECT_TRUE(result.outputPath.string().find("my_export.parquet.csv") != std::string::npos);
}

TEST_F(ParquetExporterTest, CsvHeaderHasCorrectColumns)
{
    auto result = exporter.exportData(storage, analysisResult, tmpDir, nullptr);
    ASSERT_TRUE(result.success);

    auto content = readFile(result.outputPath);
    // Header lines (comment + schema + column names = 3 lines).
    // Column header must be the last non-data line.
    EXPECT_TRUE(content.find("trajectory_id,step_index,x_cm,y_cm,z_cm,edep_MeV,kinE_MeV")
                != std::string::npos);
}

TEST_F(ParquetExporterTest, DataRowsMatchInput)
{
    auto result = exporter.exportData(storage, analysisResult, tmpDir, nullptr);
    ASSERT_TRUE(result.success);

    auto content = readFile(result.outputPath);
    std::stringstream ss(content);
    std::string line;

    // Skip header lines.
    while (std::getline(ss, line)) {
        if (line.empty() || line[0] == '#') continue;
        // Found the header row — the next line is data.
        break;
    }

    // Read first data row.
    ASSERT_TRUE(std::getline(ss, line));
    EXPECT_TRUE(line.find("1,0,") == 0) << "Expected trajectory_id=1, got: " << line;
    EXPECT_TRUE(line.find("0.01,") != std::string::npos) << "Expected x_cm=0.01";
}

TEST_F(ParquetExporterTest, ProgressGoesFromZeroToOne)
{
    struct ProgressCapture {
        std::vector<float> values;
        std::vector<std::string> stages;
    };
    ProgressCapture cap;

    ExportProgressCallback cb = [&](float pct, const std::string& stage) {
        cap.values.push_back(pct);
        cap.stages.push_back(stage);
    };

    exporter.exportData(storage, analysisResult, tmpDir, cb);

    // Should have at least start and end progress.
    ASSERT_GE(cap.values.size(), 2u);
    EXPECT_NEAR(cap.values.front(), 0.0f, 0.01f);
    EXPECT_NEAR(cap.values.back(), 1.0f, 0.01f);

    // Should be monotonically increasing.
    for (std::size_t i = 1; i < cap.values.size(); ++i) {
        EXPECT_GE(cap.values[i], cap.values[i - 1]);
    }
}

TEST_F(ParquetExporterTest, ExportWithEmptyStorage)
{
    ParquetMockStorage emptyStorage;
    auto result = exporter.exportData(emptyStorage, analysisResult, tmpDir, nullptr);

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(fs::exists(result.outputPath));
    // File should contain only header lines.
    auto content = readFile(result.outputPath);
    auto lines = std::count(content.begin(), content.end(), '\n');
    EXPECT_EQ(lines, 3);  // 2 comment lines + 1 header line
}

TEST_F(ParquetExporterTest, ExportWithManySamples)
{
    ParquetMockStorage bigStorage;
    for (int i = 0; i < 250000; ++i) {
        TrajectorySample s;
        s.trajectory_id = TrajectoryId(static_cast<uint64_t>(i % 100 + 1));
        s.position_m = {0.01, 0.02, static_cast<double>(i) * 1e-6};
        s.edep_MeV = 0.01;
        s.kinE_MeV = 100.0;
        bigStorage.samples.push_back(s);
    }

    int progressCalls = 0;
    ExportProgressCallback cb = [&](float, const std::string&) { ++progressCalls; };

    auto result = exporter.exportData(bigStorage, analysisResult, tmpDir, cb);
    EXPECT_TRUE(result.success);
    EXPECT_GT(result.bytesWritten, 1000u);
    EXPECT_GE(progressCalls, 2);  // start + end + intermediate
}

TEST_F(ParquetExporterTest, WarningMessageLogged)
{
    // Capture stdout to verify the warning is printed.
    testing::internal::CaptureStdout();
    exporter.exportData(storage, analysisResult, tmpDir, nullptr);
    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_TRUE(output.find("WARNING") != std::string::npos);
    EXPECT_TRUE(output.find("Apache Arrow") != std::string::npos);
}

TEST_F(ParquetExporterTest, NameAndFormatAreConsistent)
{
    EXPECT_EQ(exporter.name(), "Apache Parquet");
    EXPECT_EQ(exporter.format(), "parquet");
    EXPECT_EQ(exporter.fileExtensions().size(), 2u);
    EXPECT_EQ(exporter.fileExtensions()[0], ".parquet");
    EXPECT_EQ(exporter.fileExtensions()[1], ".pq");
}
