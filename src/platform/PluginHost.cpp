#include "platform/IPlugin.h"
#include "app/ApplicationContext.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <mutex>

namespace fs = std::filesystem;

namespace beamlab::platform {

using beamlab::app::ApplicationContext;

// ══════════════════════════════════════════════════════════════════════
//  Platform-specific shared library loading
// ══════════════════════════════════════════════════════════════════════

#if defined(_WIN32)
#  define NOMINMAX
#  include <windows.h>
#  define DL_OPEN(lib)       LoadLibraryA(lib)
#  define DL_SYM(handle, fn) GetProcAddress(static_cast<HMODULE>(handle), fn)
#  define DL_CLOSE(handle)   FreeLibrary(static_cast<HMODULE>(handle))
#  define DL_ERROR_MSG       "GetLastError()"
static bool hasPluginExtension(const std::string& name)
{
    auto dot = name.rfind('.');
    if (dot == std::string::npos) return false;
    auto ext = name.substr(dot);
    return ext == ".dll" || ext == ".DLL";
}
#else
#  include <dlfcn.h>
#  define DL_OPEN(lib)       dlopen(lib, RTLD_NOW | RTLD_LOCAL)
#  define DL_SYM(handle, fn) dlsym(handle, fn)
#  define DL_CLOSE(handle)   dlclose(handle)
#  define DL_ERROR_MSG       dlerror()
static bool hasPluginExtension(const std::string& name)
{
    if (name.size() < 4) return false;
    auto ext = name.substr(name.size() - 3);
    if (ext == ".so") return true;
    if (name.size() < 7) return false;
    return name.substr(name.size() - 6) == ".dylib";
}
#endif

// ── Factory function type aliases ─────────────────────────────────────

using CreatePluginFn    = class IPlugin* (*)();

// ══════════════════════════════════════════════════════════════════════
//  PluginHost
// ══════════════════════════════════════════════════════════════════════

PluginHost::PluginHost(std::vector<fs::path> searchPaths)
    : searchPaths_(std::move(searchPaths))
{
}

PluginHost::~PluginHost()
{
    shutdownAll();
}

void PluginHost::closeHandle(void* handle)
{
    if (handle) {
        DL_CLOSE(handle);
    }
}

// ── Scanning ────────────────────────────────────────────────────────

void PluginHost::scanDirectory(const fs::path& path)
{
    if (!fs::is_directory(path)) {
        std::cerr << "[PluginHost] Not a directory: " << path << std::endl;
        return;
    }

    int loaded = 0;
    for (const auto& entry : fs::directory_iterator(path)) {
        if (!entry.is_regular_file()) continue;
        auto filename = entry.path().filename().string();
        if (!hasPluginExtension(filename)) continue;

        if (auto plugin = load(entry.path())) {
            std::lock_guard<std::mutex> lock(mutex_);
            plugins_.push_back(std::move(*plugin));
            ++loaded;
        }
    }

    if (loaded > 0) {
        std::cout << "[PluginHost] Scanned " << path
                  << ": loaded " << loaded << " plugin(s)" << std::endl;
    }
}

// ── Loading ─────────────────────────────────────────────────────────

std::optional<PluginHost::LoadedPlugin>
PluginHost::load(const fs::path& libPath)
{
    auto absPath = fs::absolute(libPath).string();

    // ── Open the shared library ───────────────────────────────────
    void* handle = DL_OPEN(absPath.c_str());
    if (!handle) {
        std::cerr << "[PluginHost] dlopen failed: " << absPath
                  << " — " << DL_ERROR_MSG << std::endl;
        return std::nullopt;
    }

    // RAII guard: close handle if we return early.
    SharedLibHandle libGuard(handle);

    // ── Try factory symbols in priority order ─────────────────────
    // Plugin must export at least one of these C-linkage functions.

    // 3. create_plugin → IPlugin*
    if (auto* fn = reinterpret_cast<CreatePluginFn>(
            DL_SYM(handle, "create_plugin"))) {
        auto* instance = fn();
        if (!instance) {
            std::cerr << "[PluginHost] create_plugin() returned null: "
                      << absPath << std::endl;
            return std::nullopt;
        }
        LoadedPlugin lp;
        lp.name = instance->name();
        lp.version = instance->version();
        lp.instance.reset(instance);
        lp.sourcePath = absPath;
        libGuard.owns = false;
        handles_.emplace_back(std::move(libGuard));
        return lp;
    }

    // No known factory symbol found.
    std::cerr << "[PluginHost] No factory symbol found in: " << absPath
              << "\n  Expected: create_plugin"
              << std::endl;
    return std::nullopt;
}

// ── Registration ────────────────────────────────────────────────────

void PluginHost::registerBuiltin(std::unique_ptr<IPlugin> plugin)
{
    if (!plugin) return;
    std::lock_guard<std::mutex> lock(mutex_);
    LoadedPlugin lp;
    lp.name = plugin->name();
    lp.version = plugin->version();
    lp.instance = std::move(plugin);
    lp.sourcePath = "(builtin)";
    lp.isBuiltin = true;
    plugins_.push_back(std::move(lp));
    handles_.emplace_back();  // null handle
}

// ── Initialization ──────────────────────────────────────────────────

void PluginHost::initializeAll(ApplicationContext& ctx)
{
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& plugin : plugins_) {
        try {
            plugin.instance->initialize(ctx);
            std::cout << "[PluginHost] Initialized: " << plugin.name
                      << " v" << plugin.version
                      << "  [" << plugin.sourcePath << "]" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[PluginHost] Failed to initialize '"
                      << plugin.name << "': " << e.what() << std::endl;
        }
    }
}

// ── Shutdown ───────────────────────────────────────────────────────

void PluginHost::shutdownAll()
{
    std::lock_guard<std::mutex> lock(mutex_);

    // Shutdown in reverse order (dependencies first).
    for (auto it = plugins_.rbegin(); it != plugins_.rend(); ++it) {
        try {
            if (it->instance) {
                it->instance->shutdown();
            }
        } catch (const std::exception& e) {
            std::cerr << "[PluginHost] Shutdown error in '"
                      << it->name << "': " << e.what() << std::endl;
        }
    }

    plugins_.clear();
    handles_.clear();  // Calls SharedLibHandle destructors → dlclose
}

// ── Unload a single plugin ──────────────────────────────────────────

bool PluginHost::unload(const std::string& name)
{
    std::lock_guard<std::mutex> lock(mutex_);

    for (std::size_t i = 0; i < plugins_.size(); ++i) {
        if (plugins_[i].name == name) {
            if (plugins_[i].isBuiltin) {
                std::cerr << "[PluginHost] Cannot unload built-in plugin: "
                          << name << std::endl;
                return false;
            }

            // Shutdown.
            try { plugins_[i].instance->shutdown(); }
            catch (...) {}

            plugins_.erase(plugins_.begin() + static_cast<ptrdiff_t>(i));
            handles_.erase(handles_.begin() + static_cast<ptrdiff_t>(i));

            std::cout << "[PluginHost] Unloaded: " << name << std::endl;
            return true;
        }
    }
    return false;
}

// ── Count ────────────────────────────────────────────────────────────

std::size_t PluginHost::loadedCount() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return plugins_.size();
}

} // namespace beamlab::platform
