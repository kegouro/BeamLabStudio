// Tests for F2 fixes: S3 (PDG charge), S4 (streaming≡batch guards),
// S6 (FocusDetector plateau tie-break), G4 (round-trip IEEE-754).
//
// Each test: Arrange–Act–Assert; name: test_<unit>_<scenario>_<expected>.

#include "analysis/focus/FocusDetector.h"
#include "analysis/focus/FocusParameters.h"
#include "core/storage/InMemoryStorage.h"
#include "data/model/FocusCurve.h"
#include "data/model/FocusResult.h"
#include "data/model/TrajectoryDataset.h"
#include "data/model/TrajectorySample.h"
#include "io/exporters/VisualizationDataExporter.h"
#include "io/importers/Geant4CsvImporter.h"
#include "io/importers/IImporter.h"

#include <gtest/gtest.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace beamlab::analysis;
using namespace beamlab::data;
using namespace beamlab::io;

// ─────────────────────────────────────────────────────────────────────────────
//  Helper: write a minimal Geant4 CSV to a temp file.
// ─────────────────────────────────────────────────────────────────────────────

static std::string writeTempCsv(const std::string& header,
                                const std::vector<std::string>& rows)
{
    char tmpl[] = "/tmp/beamlab_test_XXXXXX.csv";
    // mkstemps needs the suffix length
    int fd = mkstemps(tmpl, 4);
    EXPECT_GE(fd, 0);
    close(fd);

    std::ofstream out(tmpl);
    out << header << "\n";
    for (const auto& row : rows) {
        out << row << "\n";
    }
    return tmpl;
}

// ─────────────────────────────────────────────────────────────────────────────
//  S3 — PDG charge table
// ─────────────────────────────────────────────────────────────────────────────

// Helper: import one trajectory from inline CSV, return ParticleInfo
static beamlab::data::ParticleInfo importParticleInfo(const std::string& pdg_str,
                                                       const std::string& pname)
{
    const std::string header =
        "x_cm,y_cm,z_cm,edep_MeV,kinE_MeV,"
        "momx_MeV,momy_MeV,momz_MeV,"
        "time_ns,trackID,parentID,eventID,pdg,particleName,source_file";

    // Two samples so the trajectory is valid.
    auto makeRow = [&](int track) {
        return std::string("0.0,0.0,0.0,0.1,100.0,0.0,0.0,100.0,0.0,") +
               std::to_string(track) + ",0,1," + pdg_str + ",\"" + pname + "\",test.csv";
    };

    const std::string path = writeTempCsv(header, {makeRow(1), makeRow(1)});

    Geant4CsvImporter imp;
    const auto report = imp.inspect(path);
    const auto schema = imp.buildSchema(path, report);
    beamlab::io::ImportContext ctx;
    const auto result = imp.import(path, schema, ctx);

    fs::remove(path);

    EXPECT_TRUE(result.success) << "Import failed for PDG " << pdg_str;
    if (!result.dataset || result.dataset->trajectories.empty()) {
        return {};
    }
    return result.dataset->trajectories.front().particle;
}

TEST(F2_PDGCharge, test_pdgCharge_proton_plusOne)
{
    // PDG 2212 (proton) → charge +1
    const auto p = importParticleInfo("2212", "proton");
    EXPECT_DOUBLE_EQ(p.charge, +1.0) << "Proton charge should be +1 (PDG 2022)";
}

TEST(F2_PDGCharge, test_pdgCharge_electron_minusOne)
{
    // PDG 11 (e−) → charge −1
    const auto p = importParticleInfo("11", "e-");
    EXPECT_DOUBLE_EQ(p.charge, -1.0) << "Electron charge should be −1 (PDG 2022)";
}

TEST(F2_PDGCharge, test_pdgCharge_positron_plusOne)
{
    // PDG −11 (e+) → charge +1
    const auto p = importParticleInfo("-11", "e+");
    EXPECT_DOUBLE_EQ(p.charge, +1.0) << "Positron charge should be +1 (PDG 2022)";
}

TEST(F2_PDGCharge, test_pdgCharge_photon_zero)
{
    // PDG 22 (γ) → charge 0
    const auto p = importParticleInfo("22", "gamma");
    EXPECT_DOUBLE_EQ(p.charge, 0.0) << "Photon charge should be 0 (PDG 2022)";
}

TEST(F2_PDGCharge, test_pdgCharge_neutron_zero)
{
    // PDG 2112 (n) → charge 0
    const auto p = importParticleInfo("2112", "neutron");
    EXPECT_DOUBLE_EQ(p.charge, 0.0) << "Neutron charge should be 0 (PDG 2022)";
}

TEST(F2_PDGCharge, test_pdgCharge_muon_minus_minusOne)
{
    // PDG 13 (μ−) → charge −1
    const auto p = importParticleInfo("13", "mu-");
    EXPECT_DOUBLE_EQ(p.charge, -1.0) << "Muon− charge should be −1 (PDG 2022)";
}

TEST(F2_PDGCharge, test_pdgCharge_antimuon_plusOne)
{
    // PDG −13 (μ+) → charge +1
    const auto p = importParticleInfo("-13", "mu+");
    EXPECT_DOUBLE_EQ(p.charge, +1.0) << "Anti-muon charge should be +1 (PDG 2022)";
}

// ─────────────────────────────────────────────────────────────────────────────
//  S4 — Streaming ≡ Batch: same CSV → same samples
// ─────────────────────────────────────────────────────────────────────────────

// Floating-point-safe compare for TrajectorySample positions.
static bool samplesMatch(const beamlab::data::TrajectorySample& a,
                          const beamlab::data::TrajectorySample& b)
{
    return a.position_m.x == b.position_m.x &&
           a.position_m.y == b.position_m.y &&
           a.position_m.z == b.position_m.z &&
           a.edep_MeV == b.edep_MeV &&
           a.kinE_MeV == b.kinE_MeV &&
           a.time_s == b.time_s;
}

TEST(F2_StreamingEqBatch, test_streamingAndBatch_sameCSV_identicalSamples)
{
    // Arrange: CSV with mixed valid/invalid rows (negative edep, negative kinE,
    // negative time) to exercise guards in both paths.
    const std::string header =
        "x_cm,y_cm,z_cm,edep_MeV,kinE_MeV,"
        "momx_MeV,momy_MeV,momz_MeV,"
        "time_ns,trackID,parentID,eventID,pdg,particleName,source_file";

    const std::vector<std::string> rows = {
        // event=1, track=1 — 3 valid steps
        "0.0,0.0,0.0,0.5,100.0,0.0,0.0,100.0,0.0,1,0,1,2212,proton,test.csv",
        "1.0,0.0,0.0,0.5,99.5,0.0,0.0,100.0,1.0,1,0,1,2212,proton,test.csv",
        "2.0,0.0,0.0,0.5,99.0,0.0,0.0,100.0,2.0,1,0,1,2212,proton,test.csv",
        // event=1, track=2 — negative edep (should be dropped by both paths)
        "3.0,0.0,0.0,-0.1,99.0,0.0,0.0,100.0,3.0,2,0,1,2212,proton,test.csv",
        // event=1, track=3 — negative kinE (should be dropped by both paths)
        "4.0,0.0,0.0,0.1,-1.0,0.0,0.0,100.0,4.0,3,0,1,2212,proton,test.csv",
        // event=1, track=4 — negative time (should be dropped by both paths)
        "5.0,0.0,0.0,0.1,50.0,0.0,0.0,100.0,-1.0,4,0,1,2212,proton,test.csv",
        // event=2, track=1 — 2 valid steps
        "0.0,0.0,5.0,0.3,80.0,0.0,0.0,80.0,10.0,1,0,2,2212,proton,test.csv",
        "0.0,0.0,6.0,0.3,79.7,0.0,0.0,80.0,11.0,1,0,2,2212,proton,test.csv",
    };

    const std::string csv_path = writeTempCsv(header, rows);

    // Act: batch import
    Geant4CsvImporter imp;
    const auto report = imp.inspect(csv_path);
    const auto schema = imp.buildSchema(csv_path, report);
    beamlab::io::ImportContext ctx;
    const auto batch_result = imp.import(csv_path, schema, ctx);
    ASSERT_TRUE(batch_result.success) << "Batch import failed";
    ASSERT_TRUE(batch_result.dataset.has_value());

    // Collect all batch samples in insertion order
    std::vector<beamlab::data::TrajectorySample> batch_samples;
    for (const auto& traj : batch_result.dataset->trajectories) {
        for (const auto& s : traj.samples) {
            batch_samples.push_back(s);
        }
    }

    // Act: streaming import into InMemoryStorage
    beamlab::core::InMemoryStorage stream_storage;
    const auto stream_count = imp.importStreaming(csv_path, stream_storage, nullptr);

    // Streaming samples
    std::vector<beamlab::data::TrajectorySample> stream_samples =
        stream_storage.getBatch(0, stream_storage.totalSampleCount());

    fs::remove(csv_path);

    // Assert: same sample count
    ASSERT_EQ(batch_samples.size(), stream_samples.size())
        << "Batch=" << batch_samples.size() << " stream=" << stream_samples.size();

    // Assert: stream count matches storage
    EXPECT_EQ(stream_count, stream_samples.size());

    // Assert: positions/energies match element-by-element
    for (std::size_t i = 0; i < batch_samples.size(); ++i) {
        EXPECT_TRUE(samplesMatch(batch_samples[i], stream_samples[i]))
            << "Sample " << i << " differs between batch and streaming paths";
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  S6 — FocusDetector plateau tie-break
// ─────────────────────────────────────────────────────────────────────────────

static FocusCurve makePlateau(std::initializer_list<double> vals)
{
    FocusCurve curve;
    curve.metric_name = "test_plateau";
    double ref = 0.0;
    for (double v : vals) {
        FocusCurvePoint p;
        p.reference_value = ref;
        p.metric_value = v;
        p.point_count = 10;
        curve.points.push_back(p);
        ref += 1.0;
    }
    return curve;
}

TEST(F2_FocusTieBreak, test_focusDetector_singleMinimum_picksCorrectIndex)
{
    // Arrange: unambiguous minimum at index 3
    FocusDetector det;
    FocusParameters params;
    params.smooth_curve = false;
    const auto curve = makePlateau({5.0, 4.0, 3.0, 1.0, 3.0, 4.0, 5.0});

    // Act
    const auto r = det.detect(curve, params);

    // Assert
    EXPECT_TRUE(r.valid);
    EXPECT_EQ(r.focus_index, 3u);
}

TEST(F2_FocusTieBreak, test_focusDetector_twoTiedBins_picksCentroid)
{
    // Arrange: minimum shared at indices 3 and 4 → centroid = 3 (floor of 3.5)
    FocusDetector det;
    FocusParameters params;
    params.smooth_curve = false;
    const auto curve = makePlateau({5.0, 4.0, 3.0, 1.0, 1.0, 3.0, 4.0});

    // Act
    const auto r = det.detect(curve, params);

    // Assert
    EXPECT_TRUE(r.valid);
    EXPECT_EQ(r.focus_index, 3u)
        << "Two tied bins at 3 and 4 → centroid index 3 (floor of 3.5)";
}

TEST(F2_FocusTieBreak, test_focusDetector_threeTiedBins_picksCentroid)
{
    // Arrange: plateau at indices 2, 3, 4 → centroid = 3
    FocusDetector det;
    FocusParameters params;
    params.smooth_curve = false;
    const auto curve = makePlateau({5.0, 4.0, 1.0, 1.0, 1.0, 4.0, 5.0});

    // Act
    const auto r = det.detect(curve, params);

    // Assert
    EXPECT_TRUE(r.valid);
    EXPECT_EQ(r.focus_index, 3u)
        << "Three tied bins at 2,3,4 → centroid index 3";
}

TEST(F2_FocusTieBreak, test_focusDetector_tc01Plateau_picksLowerIndex)
{
    // TC01 scenario: bins 9 and 10 share the same r_rms minimum.
    // The plateau fix should pick index 9 (centroid of {9,10}).
    const double shared_min = 0.0003603738152302427;
    FocusDetector det;
    FocusParameters params;
    params.smooth_curve = false;

    FocusCurve curve;
    curve.metric_name = "transverse_r_rms";
    for (int i = 0; i < 20; ++i) {
        FocusCurvePoint p;
        p.reference_value = static_cast<double>(i) * 0.015789473684210527;
        // Decreasing toward bins 9-10, then increasing — both exactly shared_min.
        if (i == 9 || i == 10) {
            p.metric_value = shared_min;
        } else {
            p.metric_value = shared_min + std::abs(9.5 - i) * 0.00005;
        }
        p.point_count = 50;
        curve.points.push_back(p);
    }

    const auto r = det.detect(curve, params);
    EXPECT_TRUE(r.valid);
    EXPECT_EQ(r.focus_index, 9u)
        << "TC01 plateau {9,10} → centroid 9 (not 10)";
}

// ─────────────────────────────────────────────────────────────────────────────
//  G4 — Round-trip IEEE-754 exact (std::memcmp)
// ─────────────────────────────────────────────────────────────────────────────

TEST(F2_RoundTrip, test_vizExporter_precision17_exactRoundTrip)
{
    // Arrange: representative doubles that stress 12-digit truncation
    // but survive 17-digit round-trip.
    const std::vector<double> test_values = {
        // Transcendental-ish values that expose the 12-digit precision gap
        1.0 / 3.0,               // 0.333…
        std::exp(1.0),           // e
        std::sqrt(2.0),          // √2
        0.1,                     // Classic IEEE-754 non-exact
        0.2,
        1.23456789012345e-7,
        9.87654321098765e+10,
        -3.14159265358979323846, // π (negative)
        1.0e-300,                // Subnormal-adjacent
        1.0e+300,
    };

    // Build a minimal TrajectoryDataset with one trajectory and one sample
    // per test_value (using the value as x-coordinate).
    TrajectoryDataset dataset;
    beamlab::data::Trajectory traj;
    traj.id = beamlab::data::TrajectoryId{1};
    for (double v : test_values) {
        beamlab::data::TrajectorySample s;
        s.position_m.x = v;
        s.position_m.y = v * 1.1;
        s.position_m.z = v * 0.9;
        s.edep_MeV = v;
        s.kinE_MeV = std::abs(v);
        traj.samples.push_back(s);
    }
    dataset.trajectories.push_back(std::move(traj));

    // Act: export
    char tmpl[] = "/tmp/beamlab_roundtrip_XXXXXX.csv";
    int fd = mkstemps(tmpl, 4);
    ASSERT_GE(fd, 0);
    close(fd);
    const std::string out_path = tmpl;

    VisualizationDataExporter exp;
    const auto res = exp.exportTrajectoryPreview(
        dataset, out_path,
        /*max_trajectories=*/1,
        /*max_samples_per_trajectory=*/static_cast<std::size_t>(test_values.size()));
    ASSERT_TRUE(res.success) << "Export failed: " << out_path;

    // Parse: re-read the exported values
    std::ifstream in(out_path);
    ASSERT_TRUE(in.is_open());
    std::string line;
    // Skip comment lines and header
    while (std::getline(in, line)) {
        if (!line.empty() && line[0] != '#') break;  // header line
    }
    // Now read data lines
    std::vector<double> read_x;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        std::istringstream ss(line);
        std::string tok;
        int col = 0;
        while (std::getline(ss, tok, ',')) {
            if (col == 3) {  // x_m is column index 3
                double v = 0.0;
                std::istringstream vs(tok);
                vs >> v;
                read_x.push_back(v);
                break;
            }
            ++col;
        }
    }
    in.close();
    fs::remove(out_path);

    // Assert: bit-exact round-trip for each value
    ASSERT_EQ(read_x.size(), test_values.size())
        << "Number of parsed rows doesn't match exported rows";

    for (std::size_t i = 0; i < test_values.size(); ++i) {
        double orig = test_values[i];
        double read = read_x[i];
        // memcmp on the raw bits — any ULP difference fails.
        EXPECT_EQ(std::memcmp(&orig, &read, sizeof(double)), 0)
            << "Round-trip not bit-exact for value[" << i << "]="
            << std::scientific << std::setprecision(17) << orig
            << " read=" << read;
    }
}
