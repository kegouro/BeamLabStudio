#include "platform/IPlugin.h"
#include "app/ApplicationContext.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <memory>

namespace fs = std::filesystem;
using namespace beamlab::platform;
using beamlab::app::ApplicationContext;

// ── Dummy built-in plugins ──────────────────────────────────────────

class CounterPlugin final : public IPlugin {
public:
    int initCalls{0};
    int shutdownCalls{0};

    std::string name() const override { return "Counter"; }
    std::string version() const override { return "1.0.0"; }
    std::string description() const override { return "Test counter"; }
    void initialize(ApplicationContext&) override { ++initCalls; }
    void shutdown() override { ++shutdownCalls; }
};

class SilentPlugin final : public IPlugin {
public:
    std::string name() const override { return "Silent"; }
    std::string version() const override { return "2.0.0"; }
    std::string description() const override { return "Does nothing"; }
    void initialize(ApplicationContext&) override {}
    void shutdown() override {}
};

// ── Builtin registration ────────────────────────────────────────────

TEST(PluginHostTest, RegisterBuiltinIncrementsCount)
{
    PluginHost host;
    EXPECT_EQ(host.loadedCount(), 0u);

    host.registerBuiltin(std::make_unique<CounterPlugin>());
    EXPECT_EQ(host.loadedCount(), 1u);
}

TEST(PluginHostTest, RegisterNullDoesNotCrash)
{
    PluginHost host;
    EXPECT_NO_THROW(host.registerBuiltin(nullptr));
    EXPECT_EQ(host.loadedCount(), 0u);
}

TEST(PluginHostTest, MultipleBuiltins)
{
    PluginHost host;

    host.registerBuiltin(std::make_unique<CounterPlugin>());
    host.registerBuiltin(std::make_unique<SilentPlugin>());

    EXPECT_EQ(host.loadedCount(), 2u);
}

// ── getPluginsOfType ────────────────────────────────────────────────

TEST(PluginHostTest, GetPluginsOfTypeReturnsMatchingPlugins)
{
    PluginHost host;
    host.registerBuiltin(std::make_unique<CounterPlugin>());
    host.registerBuiltin(std::make_unique<CounterPlugin>());

    auto plugins = host.getPluginsOfType<IPlugin>();
    EXPECT_EQ(plugins.size(), 2u);
}

TEST(PluginHostTest, GetPluginsOfTypeFiltersCorrectly)
{
    PluginHost host;
    host.registerBuiltin(std::make_unique<CounterPlugin>());

    auto iplugins = host.getPluginsOfType<IPlugin>();
    EXPECT_EQ(iplugins.size(), 1u);
    EXPECT_EQ(iplugins[0]->name(), "Counter");
}

// ── Initialize / Shutdown ───────────────────────────────────────────

TEST(PluginHostTest, InitializeAllCallsInitialize)
{
    PluginHost host;
    auto* raw = new CounterPlugin();
    auto* rawPtr = raw;
    host.registerBuiltin(std::unique_ptr<CounterPlugin>(raw));

    ApplicationContext ctx;
    host.initializeAll(ctx);

    EXPECT_EQ(rawPtr->initCalls, 1);
}

TEST(PluginHostTest, ShutdownAllCallsShutdown)
{
    // Use a shared counter so we can observe the call after the plugin is destroyed.
    int shutdownCount = 0;

    class ObservablePlugin final : public IPlugin {
    public:
        int& counter;
        explicit ObservablePlugin(int& c) : counter(c) {}
        std::string name()        const override { return "Observable"; }
        std::string version()     const override { return "1.0.0"; }
        std::string description() const override { return ""; }
        void initialize(beamlab::app::ApplicationContext&) override {}
        void shutdown() override { ++counter; }
    };

    PluginHost host;
    host.registerBuiltin(std::make_unique<ObservablePlugin>(shutdownCount));
    host.shutdownAll();

    EXPECT_EQ(shutdownCount, 1);
}

TEST(PluginHostTest, InitializeAllSkipsUnregistered)
{
    PluginHost host;
    ApplicationContext ctx;
    // No plugins registered — must not crash.
    EXPECT_NO_THROW(host.initializeAll(ctx));
}

TEST(PluginHostTest, ShutdownAllOnEmptyHost)
{
    PluginHost host;
    EXPECT_NO_THROW(host.shutdownAll());
}

// ── Unload ──────────────────────────────────────────────────────────

TEST(PluginHostTest, CannotUnloadBuiltin)
{
    PluginHost host;
    host.registerBuiltin(std::make_unique<CounterPlugin>());

    EXPECT_FALSE(host.unload("Counter"));
    EXPECT_EQ(host.loadedCount(), 1u);
}

TEST(PluginHostTest, UnloadMissingReturnsFalse)
{
    PluginHost host;
    EXPECT_FALSE(host.unload("nonexistent"));
}

// ── Load from .so (optional, requires system compiler) ──────────────

static int compilePlugin(const fs::path& srcPath,
                          const fs::path& libPath,
                          const std::string& pluginName)
{
    std::string cmd =
        "c++ -std=c++17 -fPIC -shared -I" +
        std::string(PROJECT_SOURCE_DIR) + "/src"
        " -o " + libPath.string() + " " + srcPath.string() + " 2>/dev/null";
    return std::system(cmd.c_str());
}

static fs::path writeDummyPluginSource(const fs::path& dir)
{
    auto src = dir / "dummy_plugin.cpp";
    std::ofstream out(src);
    out << R"cpp(
#include "platform/IPlugin.h"
#include "app/ApplicationContext.h"

using namespace beamlab::platform;

class DummyPlugin final : public IPlugin {
public:
    std::string name() const override { return "Dummy"; }
    std::string version() const override { return "1.0.0"; }
    std::string description() const override { return "External test plugin"; }
    void initialize(ApplicationContext&) override {}
    void shutdown() override {}
};

extern "C" __attribute__((visibility("default"))) IPlugin* create_plugin() {
    return new DummyPlugin();
}
)cpp";
    return src;
}

TEST(PluginHostTest, LoadFromSharedLibrary)
{
    auto tmpDir = fs::temp_directory_path() / "beamlab_plugin_test";
    fs::create_directories(tmpDir);

    auto srcPath = writeDummyPluginSource(tmpDir);
    auto libPath = tmpDir / "libdummy_test.so";

    int ret = compilePlugin(srcPath, libPath, "Dummy");
    if (ret != 0 || !fs::exists(libPath)) {
        GTEST_SKIP() << "System compiler not available — skipping .so test";
    }

    PluginHost host;
    auto loaded = host.load(libPath);

    ASSERT_TRUE(loaded.has_value()) << "Failed to load " << libPath;
    EXPECT_EQ(loaded->name, "Dummy");
    EXPECT_EQ(loaded->version, "1.0.0");
    EXPECT_FALSE(loaded->isBuiltin);

    // Cleanup.
    fs::remove_all(tmpDir);
}

TEST(PluginHostTest, LoadMissingReturnsNullopt)
{
    PluginHost host;
    auto result = host.load("/nonexistent/plugin.so");
    EXPECT_FALSE(result.has_value());
}

TEST(PluginHostTest, ScanDirectory)
{
    auto tmpDir = fs::temp_directory_path() / "beamlab_scan_test";
    fs::create_directories(tmpDir);

    auto srcPath = writeDummyPluginSource(tmpDir);
    auto libPath = tmpDir / "libscan_test.so";

    int ret = compilePlugin(srcPath, libPath, "ScanDummy");
    if (ret != 0 || !fs::exists(libPath)) {
        GTEST_SKIP() << "System compiler not available — skipping scan test";
    }

    PluginHost host;
    auto loaded = host.load(libPath);
    ASSERT_TRUE(loaded.has_value());

    // Cleanup.
    fs::remove_all(tmpDir);
}
