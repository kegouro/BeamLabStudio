#include "io/importers/Geant4CsvImporter.h"
#include "io/importers/IImporter.h"

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
    const auto path = fixturePath("simple_trajectories.csv");
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
    const auto path = fixturePath("simple_trajectories.csv");
    ASSERT_TRUE(std::filesystem::exists(path));

    Geant4CsvImporter imp;
    const auto report = imp.inspect(path);
    EXPECT_TRUE(report.readable);
    EXPECT_FALSE(report.preview_lines.empty());
}

TEST(Geant4CsvImporterTest, ImportDoesNotCrash)
{
    const auto path = fixturePath("simple_trajectories.csv");
    ASSERT_TRUE(std::filesystem::exists(path));

    Geant4CsvImporter imp;
    const auto report = imp.inspect(path);
    ASSERT_TRUE(report.readable);

    const auto schema = imp.buildSchema(path, report);
    ASSERT_FALSE(schema.schema_name.empty());

    ImportContext ctx;
    const auto result = imp.import(path, schema, ctx);
    EXPECT_TRUE(result.success) << "Import failed";
    ASSERT_TRUE(result.dataset.has_value());
    EXPECT_GT(result.dataset->trajectories.size(), 0u);

    for (const auto& traj : result.dataset->trajectories) {
        for (const auto& sample : traj.samples) {
            EXPECT_GE(sample.kinE_MeV, 0.0);
        }
    }
}

TEST(Geant4CsvImporterTest, MissingFileReturnsError)
{
    Geant4CsvImporter imp;
    const auto report = imp.inspect("/nonexistent/file.csv");
    EXPECT_FALSE(report.readable);
}
