#include "services/analysis/engines/FrameStatisticsPlugin.h"
#include "services/storage/IStorageBackend.h"
#include "services/storage/StorageManager.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <numeric>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace beamlab::services::analysis;
using namespace beamlab::services::storage;

// ─────────────────────────────────────────────────────────────────────
//  Fixture
// ─────────────────────────────────────────────────────────────────────

class PipelineRegressionTest : public ::testing::Test {
protected:
    fs::path tmpDir_;
    fs::path csvPath_;
    fs::path legacyOutputDir_;
    fs::path nativeOutputDir_;

    void SetUp() override
    {
        // Unique temp directory.
        char tmpl[] = "/tmp/beamlab_regression_XXXXXX";
        auto* dir = mkdtemp(tmpl);
        ASSERT_NE(dir, nullptr);
        tmpDir_ = fs::path(dir);

        csvPath_ = tmpDir_ / "synthetic_beam.csv";
        legacyOutputDir_ = tmpDir_ / "legacy_output";
        nativeOutputDir_ = tmpDir_ / "native_output";
        fs::create_directories(legacyOutputDir_);
        fs::create_directories(nativeOutputDir_);
    }

    void TearDown() override
    {
        if (!tmpDir_.empty() && fs::exists(tmpDir_)) {
            fs::remove_all(tmpDir_);
        }
    }
};

// ─────────────────────────────────────────────────────────────────────
//  Dataset generator
// ─────────────────────────────────────────────────────────────────────

/// Generate a CSV with N trajectories × M samples each.
/// Beam is a gaussian with waist W0 at focus z0, diverging linearly.
/// Columns: x_cm,y_cm,z_cm,edep_MeV,kinE_MeV,momx_MeV,momy_MeV,momz_MeV,
///          time_ns,trackID,parentID,eventID,pdg,particleName,source_file
static void generateRegressionDataset(const fs::path& csvPath,
                                       int nTrajectories,
                                       int nSamplesPerTrajectory)
{
    std::mt19937_64 rng(42);  // Fixed seed for reproducibility.

    // Beam parameters (SI, then convert to cm for CSV).
    const double W0 = 0.5e-3;        // Waist at focus [m]
    const double z0 = 0.15;          // Focus position [m]
    const double zRange = 0.30;      // Total z range [m]
    const double divergence = 0.05;  // Divergence half-angle [rad]
    const double meanKinE = 150.0;   // MeV
    const double meanEdep = 0.01;    // MeV per step

    std::ofstream out(csvPath);
    ASSERT_TRUE(out.is_open()) << "Cannot write " << csvPath;

    // Header (Geant4 15-column format).
    out << "x_cm,y_cm,z_cm,edep_MeV,kinE_MeV,"
           "momx_MeV,momy_MeV,momz_MeV,"
           "time_ns,trackID,parentID,eventID,pdg,particleName,source_file\n";

    for (int t = 0; t < nTrajectories; ++t) {
        // Each trajectory gets a random initial angle.
        double theta = std::normal_distribution<double>(0.0, divergence)(rng);
        double phi = std::uniform_real_distribution<double>(0.0, 2.0 * M_PI)(rng);

        double dx = std::sin(theta) * std::cos(phi);
        double dy = std::sin(theta) * std::sin(phi);

        for (int s = 0; s < nSamplesPerTrajectory; ++s) {
            double z = zRange * static_cast<double>(s) /
                       static_cast<double>(nSamplesPerTrajectory - 1);

            // Beam radius at this z (gaussian propagation).
            double zRel = (z - z0);
            double w = W0 * std::sqrt(1.0 + std::pow(zRel / (M_PI * W0 / meanKinE * 137.0), 2.0));

            // Transverse position = trajectory angle × z + random gaussian scatter.
            double sigma = w / 2.0;
            double x = dx * z + std::normal_distribution<double>(0.0, sigma)(rng);
            double y = dy * z + std::normal_distribution<double>(0.0, sigma)(rng);

            double edep = std::abs(std::normal_distribution<double>(meanEdep, meanEdep * 0.1)(rng));
            double kinE = meanKinE - edep * s * 0.01;

            // Convert to cm for CSV.
            out << (x * 100.0) << "," << (y * 100.0) << "," << (z * 100.0) << ","
                << edep << "," << std::max(kinE, 0.0) << ","
                << (dx * meanKinE) << "," << (dy * meanKinE) << ","
                << (meanKinE) << ","
                << (s * 0.1) << ","
                << (t + 1) << ",0,1,13,\"mu-\",\"synthetic_beam.csv\"\n";
        }
    }

    out.close();
}

// ─────────────────────────────────────────────────────────────────────
//  Legacy runner
// ─────────────────────────────────────────────────────────────────────

struct LegacyFrame {
    double z_min_m;        // Axial bin range
    double z_max_m;
    int point_count;
    double centroid_u;     // Transverse centroid
    double centroid_v;
    double sigma_u;        // RMS width
    double sigma_v;
    double r_rms;          // RMS radius
};

struct LegacyResults {
    std::vector<LegacyFrame> frames;
    double focusZ_m{0.0};
    double minRMSRadius_m{0.0};
};

/// Run the `beamlab` CLI and parse its energy_step_profile.csv output.
static std::optional<LegacyResults> runLegacyPipeline(
    const fs::path& csvPath,
    const fs::path& outputDir)
{
    // Check if beamlab binary exists.
    auto beamlabBin = fs::path("build") / "beamlab";
    if (!fs::exists(beamlabBin)) {
        // Try common paths.
        beamlabBin = fs::path("../build") / "beamlab";
        if (!fs::exists(beamlabBin)) {
            return std::nullopt;
        }
    }

    // Build and run command.
    std::string cmd = beamlabBin.string() + " --input " + csvPath.string()
                    + " --output " + outputDir.string()
                    + " --axis z --binning uniform --axial-bins 20";
    int ret = std::system(cmd.c_str());
    if (ret != 0) return std::nullopt;

    // Parse the energy_step_profile.csv output.
    auto profilePath = outputDir / "tables" / "energy_step_profile.csv";
    if (!fs::exists(profilePath)) return std::nullopt;

    std::ifstream in(profilePath);
    if (!in.is_open()) return std::nullopt;

    LegacyResults results;

    std::string line;
    // Skip header.
    std::getline(in, line);

    while (std::getline(in, line)) {
        if (line.empty()) continue;

        std::stringstream ss(line);
        LegacyFrame f;
        std::string token;

        // Columns: event_id, track_id, step_index, axial_m, edep_MeV, edep_eV, ...
        // We need: axial_m (z), centroid_u, centroid_v, sigma_u, sigma_v
        // The CSV may have varying columns.  Try to extract by position.
        std::vector<std::string> cols;
        while (std::getline(ss, token, ',')) {
            cols.push_back(token);
        }

        if (cols.size() < 5) continue;

        // Attempt to map columns based on known output format.
        // Format: event_id, track_id, step_index, axial_m, edep_MeV, ...
        // Focus: r_rms min is our proxy.
        f.z_min_m = std::stod(cols[3]);  // axial_m already in metres
        f.z_max_m = f.z_min_m;                   // Not in CSV; approximate.
        f.point_count = 0;

        results.frames.push_back(f);
    }

    in.close();

    // Compute focus from r_rms minimum → we read energy profile, not statistics.
    // For a proper comparison, try to parse statistics CSV.
    auto statsPath = outputDir / "tables" / "focus_curve.csv";
    if (fs::exists(statsPath)) {
        std::ifstream sin(statsPath);
        std::getline(sin, line);  // Header.
        double bestR = 1e10;
        while (std::getline(sin, line)) {
            if (line.empty()) continue;
            std::stringstream ss(line);
            std::vector<std::string> cols;
            while (std::getline(ss, token, ',')) cols.push_back(token);
            if (cols.size() < 2) continue;
            double z = std::stod(cols[1]);  // reference_value_m (already metres)
            double r = std::stod(cols[2]);  // metric_value_m
            if (r < bestR) {
                bestR = r;
                results.focusZ_m = z;
                results.minRMSRadius_m = bestR;
            }
        }
    }

    return results;
}

// ─────────────────────────────────────────────────────────────────────
//  Native runner
// ─────────────────────────────────────────────────────────────────────

/// Minimal in-memory storage backend for the native pipeline test.
class TestStorageBackend final : public IStorageBackend {
public:
    std::vector<beamlab::data::TrajectorySample> samples;

    std::string backendName() const override { return "TestMemory"; }

    uint64_t totalSamples() const override { return samples.size(); }
    uint64_t totalTrajectories() const override
    {
        if (samples.empty()) return 0;
        TrajectoryId last{0};
        uint64_t n = 0;
        for (const auto& s : samples) {
            if (s.trajectory_id != last) { ++n; last = s.trajectory_id; }
        }
        return n;
    }

    void beginWriteBatch() override {}
    void writeSample(const beamlab::data::TrajectorySample& s) override
    {
        samples.push_back(s);
    }
    void endWriteBatch() override {}

    SampleBatch readBatch(uint64_t offset, uint64_t count) const override
    {
        if (offset >= samples.size()) return {nullptr, 0, offset};
        auto avail = std::min<uint64_t>(count, samples.size() - offset);
        buf_.assign(samples.begin() + static_cast<ptrdiff_t>(offset),
                    samples.begin() + static_cast<ptrdiff_t>(offset + avail));
        return {buf_.data(), avail, offset};
    }

    SampleBatch readAxialRange(double zMin, double zMax) const override
    {
        buf_.clear();
        for (const auto& s : samples) {
            if (s.position_m.z >= zMin && s.position_m.z <= zMax)
                buf_.push_back(s);
        }
        return {buf_.data(), buf_.size(), 0};
    }

    SampleBatch readTrajectory(TrajectoryId id) const override
    {
        buf_.clear();
        for (const auto& s : samples) {
            if (s.trajectory_id == id) buf_.push_back(s);
        }
        return {buf_.data(), buf_.size(), 0};
    }

    GlobalStats globalStats() const override
    {
        GlobalStats st;
        st.count = samples.size();
        if (samples.empty()) return st;
        st.zMin = samples.front().position_m.z;
        st.zMax = samples.back().position_m.z;
        for (const auto& s : samples) st.edepSum += s.edep_MeV;
        return st;
    }

    void vacuum() override {}
    uint64_t diskUsage() const override { return 0; }

private:
    mutable std::vector<beamlab::data::TrajectorySample> buf_;
};

/// Import CSV into TestStorageBackend.
static void importCsv(const fs::path& csvPath, TestStorageBackend& storage)
{
    std::ifstream in(csvPath);
    ASSERT_TRUE(in.is_open()) << "Cannot open " << csvPath;

    std::string line;
    std::getline(in, line);  // Skip header.

    while (std::getline(in, line)) {
        if (line.empty()) continue;

        std::stringstream ss(line);
        std::string token;
        std::vector<double> values;

        while (std::getline(ss, token, ',')) {
            values.push_back(std::stod(token));
        }

        if (values.size() < 15) continue;

        // Columns: x_cm, y_cm, z_cm, edep_MeV, kinE_MeV, momx, momy, momz,
        //          time_ns, trackID, parentID, eventID, pdg, particleName, source_file
        beamlab::data::TrajectorySample sample;
        sample.position_m.x = values[0] / 100.0;  // cm → m
        sample.position_m.y = values[1] / 100.0;
        sample.position_m.z = values[2] / 100.0;
        sample.edep_MeV = values[3];
        sample.kinE_MeV = values[4];
        sample.trajectory_id = TrajectoryId(static_cast<uint64_t>(values[9]));

        storage.writeSample(sample);
    }

    in.close();
}

/// Run the native FrameStatisticsPlugin and extract frame data.
static std::optional<LegacyResults> runNativePipeline(
    const fs::path& csvPath)
{
    // Import CSV into storage.
    auto storage = std::make_unique<TestStorageBackend>();
    importCsv(csvPath, *storage);

    if (storage->totalSamples() == 0) return std::nullopt;

    // Build config.
    nlohmann::json config;
    config["axis_frame"] = {
        {"origin",       {{"x", 0}, {"y", 0}, {"z", 0}}},
        {"longitudinal", {{"x", 0}, {"y", 0}, {"z", 1}}},
        {"transverse_u", {{"x", 1}, {"y", 0}, {"z", 0}}},
        {"transverse_v", {{"x", 0}, {"y", 1}, {"z", 0}}},
    };
    config["bin_count"] = 20;

    // Run engine.
    FrameStatisticsPlugin engine;
    auto result = engine.execute(*storage, config, nullptr);

    if (!result.success) return std::nullopt;

    // Parse frame data from metrics.
    LegacyResults native;
    if (!result.metrics.contains("frames_data")) return std::nullopt;

    for (const auto& f : result.metrics["frames_data"]) {
        LegacyFrame lf;
        lf.z_min_m = f["reference_min_value"].get<double>();
        lf.z_max_m = f["reference_max_value"].get<double>();
        lf.point_count = f["point_count"].get<int>();
        lf.centroid_u = f["centroid_u"].get<double>();
        lf.centroid_v = f["centroid_v"].get<double>();
        lf.sigma_u = f["sigma_u"].get<double>();
        lf.sigma_v = f["sigma_v"].get<double>();
        lf.r_rms = f["r_rms"].get<double>();
        native.frames.push_back(lf);
    }

    // Compute focus (bin with minimum r_rms).
    for (const auto& f : native.frames) {
        if (f.r_rms < native.minRMSRadius_m) {
            native.minRMSRadius_m = f.r_rms;
            native.focusZ_m = (f.z_min_m + f.z_max_m) / 2.0;
        }
    }

    return native;
}

// ─────────────────────────────────────────────────────────────────────
//  Comparison helpers
// ─────────────────────────────────────────────────────────────────────

static std::string formatDiff(double legacy, double native_)
{
    std::stringstream ss;
    ss << "legacy=" << legacy << " native=" << native_
       << " diff=" << std::abs(legacy - native_);
    return ss.str();
}

static bool compareFocusResults(const LegacyResults& legacy,
                                 const LegacyResults& native_,
                                 std::string& error)
{
    // Focus Z position: ±1e-6 m.
    if (std::abs(legacy.focusZ_m - native_.focusZ_m) > 1e-6) {
        error = "Focus Z mismatch: " + formatDiff(legacy.focusZ_m, native_.focusZ_m);
        return false;
    }

    // Min RMS radius: ±1e-6 m.
    if (std::abs(legacy.minRMSRadius_m - native_.minRMSRadius_m) > 1e-6) {
        error = "Min RMS radius mismatch: "
                + formatDiff(legacy.minRMSRadius_m, native_.minRMSRadius_m);
        return false;
    }

    return true;
}

// ─────────────────────────────────────────────────────────────────────
//  Test
// ─────────────────────────────────────────────────────────────────────

TEST_F(PipelineRegressionTest, NativeVsLegacy)
{
    // ── Generate dataset ───────────────────────────────────────────
    int nTrajectories = 100;
    int nSamplesPerTrajectory = 100;
    generateRegressionDataset(csvPath_, nTrajectories, nSamplesPerTrajectory);

    ASSERT_TRUE(fs::file_size(csvPath_) > 0)
        << "Generated CSV is empty: " << csvPath_;

    // ── Run legacy pipeline ────────────────────────────────────────
    GTEST_FLAG_SET(death_test_style, "threadsafe");
    auto legacyResults = runLegacyPipeline(csvPath_, legacyOutputDir_);

    if (!legacyResults.has_value()) {
        GTEST_SKIP() << "Legacy pipeline not available (beamlab CLI missing or failed). "
                     << "Install beamlab CLI or ensure 'build/beamlab' exists.";
    }

    // ── Run native pipeline ────────────────────────────────────────
    auto nativeResults = runNativePipeline(csvPath_);

    ASSERT_TRUE(nativeResults.has_value())
        << "Native pipeline returned no results";

    // ── Compare focus (z position + r_rms) ─────────────────────────
    std::string error;
    bool focusOk = compareFocusResults(*legacyResults, *nativeResults, error);
    EXPECT_TRUE(focusOk) << error;

    // ── Debug output on failure ────────────────────────────────────
    if (!focusOk) {
        std::cout << "\n=== Legacy frames (" << legacyResults->frames.size() << ") ===\n";
        for (const auto& f : legacyResults->frames) {
            std::cout << "  z=" << f.z_min_m << " count=" << f.point_count << "\n";
        }
        std::cout << "\n=== Native frames (" << nativeResults->frames.size() << ") ===\n";
        for (const auto& f : nativeResults->frames) {
            std::cout << "  z=" << (f.z_min_m + f.z_max_m) / 2.0
                      << " count=" << f.point_count
                      << " r_rms=" << f.r_rms << "\n";
        }
    }
}

TEST_F(PipelineRegressionTest, EmptyDatasetReturnsZeroFrames)
{
    // Edge case: empty CSV.
    auto emptyCsv = tmpDir_ / "empty.csv";
    {
        std::ofstream out(emptyCsv);
        out << "x_cm,y_cm,z_cm,edep_MeV,kine_MeV,"
               "momx_MeV,momy_MeV,momz_MeV,"
               "time_ns,trackID,parentID,eventID,pdg,particleName,source_file\n";
    }

    auto result = runNativePipeline(emptyCsv);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->frames.empty());
}

TEST_F(PipelineRegressionTest, SingleTrajectoryProducesNoValidBins)
{
    // Single trajectory with only 2 samples → no bin reaches min(3) count.
    generateRegressionDataset(csvPath_, 1, 2);

    auto result = runNativePipeline(csvPath_);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->frames.empty())
        << "Expected 0 frames (count < 3 per bin), got " << result->frames.size();
}

TEST_F(PipelineRegressionTest, MetricsArePopulated)
{
    generateRegressionDataset(csvPath_, 5, 50);

    // Run native pipeline and extract metrics.
    auto storage = std::make_unique<TestStorageBackend>();
    importCsv(csvPath_, *storage);

    nlohmann::json config;
    config["axis_frame"] = {
        {"origin",       {{"x", 0}, {"y", 0}, {"z", 0}}},
        {"longitudinal", {{"x", 0}, {"y", 0}, {"z", 1}}},
        {"transverse_u", {{"x", 1}, {"y", 0}, {"z", 0}}},
        {"transverse_v", {{"x", 0}, {"y", 1}, {"z", 0}}},
    };
    config["bin_count"] = 10;

    FrameStatisticsPlugin engine;
    auto result = engine.execute(*storage, config, nullptr);

    ASSERT_TRUE(result.success);
    ASSERT_TRUE(result.metrics.contains("frames_data"));
    ASSERT_TRUE(result.metrics.contains("total_samples"));
    ASSERT_TRUE(result.metrics.contains("s_min"));
    ASSERT_TRUE(result.metrics.contains("s_max"));

    EXPECT_GT(result.metrics["total_samples"].get<uint64_t>(), 0u);
    EXPECT_TRUE(result.metrics["s_max"].get<double>() >
                result.metrics["s_min"].get<double>());

    // Check frame-level fields.
    for (const auto& f : result.metrics["frames_data"]) {
        EXPECT_TRUE(f["valid"].get<bool>());
        EXPECT_GE(f["point_count"].get<int>(), 3);
        EXPECT_TRUE(f.contains("centroid_u"));
        EXPECT_TRUE(f.contains("centroid_v"));
        EXPECT_TRUE(f.contains("sigma_u"));
        EXPECT_TRUE(f.contains("r_rms"));
    }
}
