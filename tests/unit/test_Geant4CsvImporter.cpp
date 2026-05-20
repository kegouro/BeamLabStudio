#include "io/importers/Geant4CsvImporter.h"
#include "io/importers/IImporter.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

using namespace beamlab::io;

static std::string fixturePath(const std::string& name)
{
    return std::string(TEST_DATA_DIR) + "/" + name;
}

TEST(Geant4CsvImporterTest, NameAndExtensions)
{
    Geant4CsvImporter imp;
    EXPECT_EQ(imp.name(), "Geant4CsvImporter");
    const auto exts = imp.supportedExtensions();
    ASSERT_FALSE(exts.empty());
    EXPECT_TRUE(std::find(exts.begin(), exts.end(), ".csv") != exts.end());
}

TEST(Geant4CsvImporterTest, ProbeValidFile)
{
    const auto path = fixturePath("test_geant4_stepping.csv");
    ASSERT_TRUE(std::filesystem::exists(path)) << "Fixture not found: " << path;

    Geant4CsvImporter imp;
    ProbeResult probe;
    probe.file_path = path;
    probe.extension = ".csv";
    probe.readable = true;

    const auto score = imp.probe(probe);
    EXPECT_GT(score.confidence, 0.0);
}

TEST(Geant4CsvImporterTest, InspectValidFile)
{
    const auto path = fixturePath("test_geant4_stepping.csv");
    ASSERT_TRUE(std::filesystem::exists(path));

    Geant4CsvImporter imp;
    const auto report = imp.inspect(path);
    EXPECT_TRUE(report.readable);
    EXPECT_FALSE(report.preview_lines.empty());
}

TEST(Geant4CsvImporterTest, ImportSteppingCSV)
{
    const auto path = fixturePath("test_geant4_stepping.csv");
    ASSERT_TRUE(std::filesystem::exists(path));

    Geant4CsvImporter imp;
    const auto report = imp.inspect(path);
    ASSERT_TRUE(report.readable);

    const auto schema = imp.buildSchema(path, report);
    ASSERT_FALSE(schema.schema_name.empty());

    ImportContext ctx;
    const auto result = imp.import(path, schema, ctx);
    ASSERT_TRUE(result.success) << "Import failed";
    ASSERT_TRUE(result.dataset.has_value());

    // Exactly 1 trajectory with 3 samples
    ASSERT_EQ(result.dataset->trajectories.size(), 1u);
    const auto& traj = result.dataset->trajectories[0];
    EXPECT_EQ(traj.samples.size(), 3u);

    // First step values
    const auto& s0 = traj.samples[0];
    EXPECT_NEAR(s0.position_m.x, -2.375818637068206 * 0.01, 1e-12);
    EXPECT_NEAR(s0.position_m.y,  0.6051152696370063 * 0.01, 1e-12);
    EXPECT_NEAR(s0.position_m.z, 1164.2857775084378 * 0.01, 1e-9);
    EXPECT_NEAR(s0.edep_MeV,  0.020709936648369703, 1e-12);
    EXPECT_NEAR(s0.kinE_MeV, 1999.9792900633515, 1e-9);

    // Particle info from pdg -13 (mu+)
    EXPECT_EQ(traj.particle.event_id, 0);
    EXPECT_EQ(traj.particle.track_id, 2);
    EXPECT_EQ(traj.particle.particle_type, "mu+");
    EXPECT_GT(traj.particle.charge, 0.0); // pdg -13 → positive charge
    EXPECT_NEAR(traj.particle.mass_MeV, 105.6583755, 1e-9);
}

TEST(Geant4CsvImporterTest, MissingFileReturnsError)
{
    Geant4CsvImporter imp;
    const auto report = imp.inspect("/nonexistent/file.csv");
    EXPECT_FALSE(report.readable);
}

TEST(Geant4CsvImporterTest, BadHeaderRejected)
{
    Geant4CsvImporter imp;
    ImportSchema schema;
    schema.schema_name = "geant4_csv";
    schema.delimiter = ',';

    ImportContext ctx;
    const auto result = imp.import("/nonexistent/file.csv", schema, ctx);
    EXPECT_FALSE(result.success);
}
