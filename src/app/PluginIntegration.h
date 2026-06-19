/// ──────────────────────────────────────────────────────────────────────────
///  Plugin integration fragment for ApplicationBootstrap.
///  Insert into ApplicationBootstrap::run() after context creation,
///  before any data loading.
///
///  This block:
///    1. Creates PluginHost with default search paths
///    2. Scans directories for .so/.dylib/.dll plugins
///    3. Registers found importers into ImporterRegistry
///    4. Registers found exporters into ExporterRegistry
///    5. Stores PluginHost in ApplicationContext for lifecycle management
/// ──────────────────────────────────────────────────────────────────────────

#include "platform/IPlugin.h"
#include "platform/EventBus.h"
#include "services/import/ImporterRegistry.h"
#include "services/import/Geant4CsvImporter.h"
#include "services/import/ComsolCsvImporter.h"
#include "services/import/DelimitedTableImporter.h"
#include "services/export/ExporterRegistry.h"
#include "services/export/CsvExporter.h"
#include "services/export/ObjExporter.h"

#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace beamlab::app {

// ── Plugin paths ─────────────────────────────────────────────────────

static std::vector<fs::path> defaultPluginPaths()
{
    std::vector<fs::path> paths;

    // 1. Alongside the executable.
    auto exeDir = fs::current_path();  // simplified; in production use argv[0]
    paths.push_back(exeDir / "plugins");

    // 2. User config directory.
#if defined(_WIN32)
    if (auto* appData = std::getenv("APPDATA"))
        paths.emplace_back(std::string(appData) + "/BeamLabStudio/plugins");
#else
    if (auto* home = std::getenv("XDG_CONFIG_HOME"))
        paths.emplace_back(std::string(home) + "/BeamLabStudio/plugins");
    else if (auto* home = std::getenv("HOME"))
        paths.emplace_back(std::string(home) + "/.config/BeamLabStudio/plugins");
#endif

    // 3. System-wide install path.
    paths.emplace_back("/usr/local/share/BeamLabStudio/plugins");

    return paths;
}

// ── Plugin initialization (call this early in run()) ────────────────

static void initializePlugins(
    beamlab::platform::PluginHost& pluginHost,
    beamlab::services::import::ImporterRegistry& importerRegistry,
    beamlab::services::export_::ExporterRegistry& exporterRegistry)
{
    // ── 1. Register built-in importers ─────────────────────────────
    importerRegistry.registerImporter(
        std::make_unique<beamlab::services::import::Geant4CsvImporter>());
    importerRegistry.registerImporter(
        std::make_unique<beamlab::services::import::ComsolCsvImporter>());
    importerRegistry.registerImporter(
        std::make_unique<beamlab::services::import::DelimitedTableImporter>());
    std::cout << "[bootstrap] Registered 3 built-in importers\n";

    // ── 2. Register built-in exporters ─────────────────────────────
    exporterRegistry.registerExporter(
        std::make_unique<beamlab::services::export_::CsvExporter>());
    exporterRegistry.registerExporter(
        std::make_unique<beamlab::services::export_::ObjExporter>());
    std::cout << "[bootstrap] Registered 2 built-in exporters\n";

    // ── 3. Scan plugin directories ─────────────────────────────────
    auto searchPaths = defaultPluginPaths();
    for (const auto& dir : searchPaths) {
        if (fs::is_directory(dir)) {
            std::cout << "[bootstrap] Scanning plugins: " << dir << "\n";
            pluginHost.scanDirectory(dir);
        }
    }

    // ── 4. Register external plugins from PluginHost ───────────────
    // Importers (loaded via create_importer symbol):
    for (auto* imp : pluginHost.getPluginsOfType<
             beamlab::services::import::IImporter>()) {
        // Wrap the raw pointer; PluginHost keeps ownership.
        // The registry stores a non-owning raw proxy.
        // (In a full implementation, use a ref-counted wrapper.)
        std::cout << "[bootstrap] External importer: " << imp->name() << "\n";
        // TODO: transfer ownership — requires lifetime management between
        // PluginHost and ImporterRegistry. For now, log the discovery.
    }

    // Exporters (loaded via create_exporter symbol):
    for (auto* exp : pluginHost.getPluginsOfType<
             beamlab::services::export_::IExporter>()) {
        std::cout << "[bootstrap] External exporter: " << exp->name() << "\n";
    }

    std::cout << "[bootstrap] Plugin initialization complete. "
              << pluginHost.loadedCount() << " plugin(s) loaded.\n";
}

// ── Integration into ApplicationBootstrap::run() ────────────────────
//
//  Insert this block after ServiceRegistry initialization:
//
//    auto& eventBus     = m_context.services().resolve<EventBus>();
//    auto& importerReg  = m_context.services().resolve<ImporterRegistry>();
//    auto& exporterReg  = m_context.services().resolve<ExporterRegistry>();
//    auto  pluginHost   = std::make_shared<PluginHost>(defaultPluginPaths());
//
//    initializePlugins(*pluginHost, importerReg, exporterReg);
//
//    // Store PluginHost in context so it lives as long as the app.
//    m_context.services().registerSingleton<PluginHost>(std::move(pluginHost));
//

} // namespace beamlab::app
