#pragma once

#include "app/ApplicationContext.h"

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace beamlab::platform {

// ── IPlugin ────────────────────────────────────────────────────────────

class IPlugin {
public:
    virtual ~IPlugin() = default;
    virtual std::string name() const = 0;
    virtual std::string version() const = 0;
    virtual std::string description() const = 0;
    virtual void initialize(beamlab::app::ApplicationContext& ctx) = 0;
    virtual void shutdown() = 0;
};

// ── PluginHost ─────────────────────────────────────────────────────────

class PluginHost {
public:
    struct LoadedPlugin {
        std::string name;
        std::string version;
        std::unique_ptr<IPlugin> instance;
        std::filesystem::path sourcePath;
        bool isBuiltin{false};
    };

    /// Construct with a list of directories to scan for plugins.
    explicit PluginHost(
        std::vector<std::filesystem::path> searchPaths = {});

    ~PluginHost();

    PluginHost(const PluginHost&) = delete;
    PluginHost& operator=(const PluginHost&) = delete;

    /// Scan a directory for shared library files (.so/.dylib/.dll)
    /// and attempt to load each one as a plugin.
    void scanDirectory(const std::filesystem::path& path);

    /// Load a single plugin from a shared library path.
    /// Tries these symbols in order:
    ///   1. create_importer  → beamlab::services::import::IImporter*
    ///   2. create_exporter  → beamlab::services::export_::IExporter*
    ///   3. create_plugin    → beamlab::platform::IPlugin*
    std::optional<LoadedPlugin> load(
        const std::filesystem::path& libPath);

    /// Register a statically-compiled plugin (no dlopen needed).
    void registerBuiltin(std::unique_ptr<IPlugin> plugin);

    /// Call initialize() on every loaded and builtin plugin.
    void initializeAll(beamlab::app::ApplicationContext& ctx);

    /// Call shutdown() on every plugin and unload all shared libraries.
    void shutdownAll();

    /// Downcast-safe retrieval of plugins by interface type.
    template<typename T>
    std::vector<T*> getPluginsOfType()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<T*> result;
        for (auto& p : plugins_) {
            if (auto* casted = dynamic_cast<T*>(p.instance.get())) {
                result.push_back(casted);
            }
        }
        return result;
    }

    /// Unload a specific plugin by name.  Returns false if not found.
    bool unload(const std::string& name);

    /// Number of loaded (non-unloaded) plugins.
    std::size_t loadedCount() const;

private:
    struct SharedLibHandle {
        void* handle{nullptr};
        bool owns{true};
        ~SharedLibHandle() { if (handle && owns) closeHandle(handle); }
        SharedLibHandle() = default;
        SharedLibHandle(void* h) : handle(h) {}
        SharedLibHandle(SharedLibHandle&& o) noexcept
            : handle(o.handle), owns(o.owns) { o.handle = nullptr; }
        SharedLibHandle& operator=(SharedLibHandle&& o) noexcept
        {
            if (this != &o) {
                if (handle && owns) closeHandle(handle);
                handle = o.handle; owns = o.owns;
                o.handle = nullptr;
            }
            return *this;
        }
    };

    std::vector<std::filesystem::path> searchPaths_;
    std::vector<LoadedPlugin> plugins_;
    std::vector<SharedLibHandle> handles_;  // same order as plugins_
    mutable std::mutex mutex_;

    static void closeHandle(void* handle);
};

} // namespace beamlab::platform
