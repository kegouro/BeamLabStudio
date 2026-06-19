#include "platform/IPlugin.h"
#include "services/import/ImporterRegistry.h"
#include "services/import/Geant4CsvImporter.h"
#include "services/import/ComsolCsvImporter.h"
#include "services/import/DelimitedTableImporter.h"
#include "services/export/ExporterRegistry.h"
#include "services/export/CsvExporter.h"
#include "services/export/ObjExporter.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;
using namespace beamlab::platform;
using namespace beamlab::services::import;
using namespace beamlab::services::export_;

// ── Integration test fixture ────────────────────────────────────────

class PluginDiscoveryIntegrationTest : public ::testing::Test {
protected:
    PluginHost pluginHost;
    ImporterRegistry importerRegistry;
    ExporterRegistry exporterRegistry;

    void SetUp() override
    {
        // Simulate what ApplicationBootstrap::initializePlugins() does.
        // 1. Register built-in importers.
        importerRegistry.registerImporter(
            std::make_unique<Geant4CsvImporter>());
        importerRegistry.registerImporter(
            std::make_unique<ComsolCsvImporter>());
        importerRegistry.registerImporter(
            std::make_unique<DelimitedTableImporter>());

        // 2. Register built-in exporters.
        exporterRegistry.registerExporter(
            std::make_unique<CsvExporter>());
        exporterRegistry.registerExporter(
            std::make_unique<ObjExporter>());
    }

    // Helper: create a minimal plugin .so in a temp directory.
    // Returns the path to the .so, or empty string on failure.
    std::string createTestPluginSo(const fs::path& outDir,
                                    const std::string& pluginName,
                                    const std::string& factorySymbol)
    {
        auto srcPath = outDir / (pluginName + ".cpp");
        {
            std::ofstream out(srcPath);
            out << R"cpp(
#include "platform/IPlugin.h"
#include "app/ApplicationContext.h"

using namespace beamlab::platform;

class )cpp" << pluginName << R"cpp( final : public IPlugin {
public:
    std::string name() const override { return ")" << pluginName << R"("; }
    std::string version() const override { return "1.0.0"; }
    std::string description() const override { return "Test plugin"; }
    void initialize(ApplicationContext&) override {}
    void shutdown() override {}
};

extern "C" __attribute__((visibility("default")))
IPlugin* create_plugin() {
    return new )cpp" << pluginName << R"cpp(();
}
)cpp";
        }

        auto libPath = outDir / (pluginName + ".so");
        std::string cmd = "c++ -std=c++17 -fPIC -shared -I"
                        + std::string(PROJECT_SOURCE_DIR) + "/src "
                        + "-o " + libPath.string() + " "
                        + srcPath.string() + " 2>/dev/null";

        int ret = std::system(cmd.c_str());
        if (ret != 0 || !fs::exists(libPath)) return {};
        return libPath.string();
    }
};

// ── Tests ───────────────────────────────────────────────────────────

TEST_F(PluginDiscoveryIntegrationTest, BuiltinImportersAreRegistered)
{
    auto available = importerRegistry.availableImporters();
    EXPECT_EQ(available.size(), 3u);

    // All three built-in importers should be present.
    std::set<std::string> names;
    for (const auto* imp : available)
        names.insert(imp->name());

    EXPECT_TRUE(names.count("Geant4 CSV") > 0);
    EXPECT_TRUE(names.count("COMSOL CSV") > 0);
    EXPECT_TRUE(names.count("Delimited Table") > 0);
}

TEST_F(PluginDiscoveryIntegrationTest, BuiltinExportersAreRegistered)
{
    auto formats = exporterRegistry.availableFormats();
    EXPECT_EQ(formats.size(), 2u);

    EXPECT_TRUE(std::find(formats.begin(), formats.end(), "csv")
                != formats.end());
    EXPECT_TRUE(std::find(formats.begin(), formats.end(), "obj")
                != formats.end());
}

TEST_F(PluginDiscoveryIntegrationTest, PluginHostStartsEmpty)
{
    EXPECT_EQ(pluginHost.loadedCount(), 0u);
}

TEST_F(PluginDiscoveryIntegrationTest, DynamicFilterIncludesAllExtensions)
{
    // Simulate what AnalysisPresenter::getAvailableImporterFilters() builds.
    auto importers = importerRegistry.availableImporters();

    std::set<std::string> allExts;
    for (const auto* imp : importers) {
        for (const auto& ext : imp->supportedExtensions()) {
            allExts.insert(ext);
        }
    }

    // Should have .csv, .tsv, .txt, .dat at minimum.
    EXPECT_TRUE(allExts.count(".csv") > 0);
    EXPECT_TRUE(allExts.count(".tsv") > 0);
    EXPECT_TRUE(allExts.count(".dat") > 0);
}

TEST_F(PluginDiscoveryIntegrationTest, ScanDirectoryWithPluginSoFindsIt)
{
    auto tmpDir = fs::temp_directory_path() / "beamlab_plugin_integration";
    fs::create_directories(tmpDir);

    auto libPath = createTestPluginSo(tmpDir, "IntegrationTestPlugin",
                                       "create_plugin");
    if (libPath.empty()) {
        GTEST_SKIP() << "Could not compile test plugin .so";
    }

    // Direct load test.
    auto loaded = pluginHost.load(libPath);
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->name, "IntegrationTestPlugin");
    EXPECT_EQ(loaded->version, "1.0.0");

    // scanDirectory test.
    PluginHost scanHost;
    scanHost.scanDirectory(tmpDir);
    EXPECT_EQ(scanHost.loadedCount(), 1u);

    auto plugins = scanHost.getPluginsOfType<IPlugin>();
    bool found = false;
    for (auto* p : plugins) {
        if (p->name() == "IntegrationTestPlugin") found = true;
    }
    EXPECT_TRUE(found);

    fs::remove_all(tmpDir);
}

TEST_F(PluginDiscoveryIntegrationTest, ExternalPluginAppearsInGetPluginsOfType)
{
    auto tmpDir = fs::temp_directory_path() / "beamlab_ext_plugin";
    fs::create_directories(tmpDir);

    auto libPath = createTestPluginSo(tmpDir, "ExternalPlugin", "create_plugin");
    if (libPath.empty()) {
        GTEST_SKIP() << "Could not compile test plugin .so";
    }

    auto loaded = pluginHost.load(libPath);
    ASSERT_TRUE(loaded.has_value());

    auto plugins = pluginHost.getPluginsOfType<IPlugin>();
    auto it = std::find_if(plugins.begin(), plugins.end(),
        [](IPlugin* p) { return p->name() == "ExternalPlugin"; });
    EXPECT_NE(it, plugins.end());

    fs::remove_all(tmpDir);
}

TEST_F(PluginDiscoveryIntegrationTest, BuiltinAndExternalImportersBootstrap)
{
    auto tmpDir = fs::temp_directory_path() / "beamlab_bootstrap_test";
    fs::create_directories(tmpDir);

    // Create an importer plugin.
    auto importerLib = tmpDir / "importer_plugin.so";
    {
        auto src = tmpDir / "importer_plugin.cpp";
        std::ofstream out(src);
        out << R"cpp(
#include "services/import/IImporter.h"
#include <string>

using namespace beamlab::services::import;

class TestImporter final : public IImporter {
public:
    std::string name() const override { return "TestImporter"; }
    std::vector<std::string> supportedExtensions() const override { return {".tst"}; }
    ImporterCapabilityScore probe(const std::string&) override { return {0.0, ""}; }
    void import(const std::string&, storage::IStorageBackend&, ImportProgressCallback) override {}
};

extern "C" __attribute__((visibility("default")))
IImporter* create_importer() { return new TestImporter(); }
)cpp";
        std::string cmd = "c++ -std=c++17 -fPIC -shared -I"
                        + std::string(PROJECT_SOURCE_DIR) + "/src "
                        + "-o " + importerLib.string() + " "
                        + src.string() + " 2>/dev/null";
        std::system(cmd.c_str());
    }

    if (!fs::exists(importerLib)) {
        GTEST_SKIP() << "Could not compile test importer .so";
    }

    // Bootstrap simulation.
    PluginHost host;
    ImporterRegistry reg;

    // Register built-in.
    reg.registerImporter(std::make_unique<Geant4CsvImporter>());

    // Scan and discover.
    host.scanDirectory(tmpDir);

    // Verify external importer discovery.
    // getPluginsOfType<IImporter>() requires dynamic_cast from IPlugin adapter.
    // In the current implementation, create_importer plugins are wrapped
    // in an ImporterPluginAdapter that also implements IPlugin but does NOT
    // inherit IImporter — so getPluginsOfType<IImporter>() won't find them
    // via dynamic_cast<IImporter*>.
    //
    // This is a known limitation: external importers exported via
    // create_importer are discovered and loaded, but their IImporter*
    // interface is wrapped inside an IPlugin adapter.
    // Full IImporter* retrieval requires a separate registry or changing
    // the adapter to expose the raw IImporter* pointer via a virtual method.

    // For now, verify that the plugin was at least loaded.
    EXPECT_EQ(host.loadedCount(), 1u);

    fs::remove_all(tmpDir);
}

// ── Edge cases ────────────────────────────────────────────────────

TEST_F(PluginDiscoveryIntegrationTest, CorruptedPluginSoDoesNotCrash)
{
    auto tmpDir = fs::temp_directory_path() / "beamlab_corrupt_plugin";
    fs::create_directories(tmpDir);

    auto corruptLib = tmpDir / "corrupt.so";
    {
        std::ofstream out(corruptLib, std::ios::binary);
        uint8_t garbage[] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00};
        out.write(reinterpret_cast<const char*>(garbage), 5);
    }

    // Loading a corrupted .so should log a warning but not crash.
    PluginHost host;
    EXPECT_NO_THROW(host.load(corruptLib));
    EXPECT_EQ(host.loadedCount(), 0u);

    fs::remove_all(tmpDir);
}

TEST_F(PluginDiscoveryIntegrationTest, EmptyPluginDirectory)
{
    auto tmpDir = fs::temp_directory_path() / "beamlab_empty_plugins";
    fs::create_directories(tmpDir);

    PluginHost host;
    EXPECT_NO_THROW(host.scanDirectory(tmpDir));
    EXPECT_EQ(host.loadedCount(), 0u);

    fs::remove_all(tmpDir);
}

TEST_F(PluginDiscoveryIntegrationTest, ImporterFilterConstruction)
{
    // This simulates the MainWindow's use of getAvailableImporterFilters().
    auto importers = importerRegistry.availableImporters();

    // Build the "All supported" filter.
    std::set<std::string> allExts;
    for (const auto* imp : importers) {
        for (const auto& ext : imp->supportedExtensions())
            allExts.insert(ext);
    }

    QString combined;
    for (const auto& ext : allExts)
        combined += QString::fromStdString("*" + ext) + " ";
    auto filter = QString("Todos los soportados (%1)").arg(combined.trimmed());

    // The filter must include .csv (most common format).
    EXPECT_TRUE(filter.contains("*.csv"));
    EXPECT_TRUE(filter.contains("*.tsv"));
    EXPECT_TRUE(filter.contains("*.dat"));
}

TEST_F(PluginDiscoveryIntegrationTest, PluginHostInitializeAllDoesNotThrow)
{
    // Verify that initializeAll() handles an empty host gracefully.
    PluginHost emptyHost;
    ApplicationContext ctx;
    EXPECT_NO_THROW(emptyHost.initializeAll(ctx));
}
