#include "core/config/ProfileManager.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

namespace beamlab::core {

// ── Constructor ────────────────────────────────────────────────────────

ProfileManager::ProfileManager(std::string profilesDir, std::string defaultPath)
    : profilesDir_(std::move(profilesDir))
    , defaultPath_(std::move(defaultPath))
{
}

// ── File I/O ───────────────────────────────────────────────────────────

nlohmann::json ProfileManager::readJsonFile(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        return nlohmann::json::object();
    }
    // allow_exceptions=false: parse errors yield a "discarded" value
    // (they do not throw), so it must be checked explicitly.
    auto parsed = nlohmann::json::parse(file, nullptr, false);
    if (parsed.is_discarded()) {
        std::cerr << "[ProfileManager] Failed to parse " << path << std::endl;
        return nlohmann::json::object();
    }
    return parsed;
}

// ── Deep merge ─────────────────────────────────────────────────────────

nlohmann::json ProfileManager::deepMerge(const nlohmann::json& base,
                                          const nlohmann::json& override_)
{
    if (override_.is_null() || override_.empty()) {
        return base;
    }
    if (!base.is_object() || !override_.is_object()) {
        return override_;
    }

    nlohmann::json result = base;
    for (auto it = override_.begin(); it != override_.end(); ++it) {
        if (result.contains(it.key()) && result[it.key()].is_object()
            && it->is_object()) {
            result[it.key()] = deepMerge(result[it.key()], *it);
        } else {
            result[it.key()] = *it;
        }
    }
    return result;
}

// ── Public API ─────────────────────────────────────────────────────────

nlohmann::json ProfileManager::loadProfile(const std::string& name) const
{
    return readJsonFile(profilePath(name));
}

std::vector<std::string> ProfileManager::availableProfiles() const
{
    std::vector<std::string> names;
    if (!fs::is_directory(profilesDir_)) {
        return names;
    }
    for (const auto& entry : fs::directory_iterator(profilesDir_)) {
        if (!entry.is_regular_file()) continue;
        auto ext = entry.path().extension().string();
        if (ext != ".json") continue;
        names.push_back(entry.path().stem().string());
    }
    std::sort(names.begin(), names.end());
    return names;
}

nlohmann::json ProfileManager::resolve(
    const std::string& profileName,
    const nlohmann::json& cliOverrides) const
{
    // Layer 1: defaults
    auto config = readJsonFile(defaultPath_);

    // Layer 2: profile (overrides defaults)
    auto profile = readJsonFile(profilePath(profileName));
    config = deepMerge(config, profile);

    // Layer 3: user overrides (~/.config/beamlab/<name>.json)
    auto userPath = userConfigDir() + "/" + profileName + ".json";
    auto userOverrides = readJsonFile(userPath);
    config = deepMerge(config, userOverrides);

    // Layer 4: CLI overrides (last wins)
    config = deepMerge(config, cliOverrides);

    // Stamp the profile name used
    config["_resolved_profile"] = profileName;

    return config;
}

void ProfileManager::saveUserOverrides(
    const std::string& profileName,
    const nlohmann::json& overrides) const
{
    auto dir = userConfigDir();
    fs::create_directories(dir);

    auto path = dir + "/" + profileName + ".json";
    std::ofstream file(path);
    if (!file.is_open()) {
        std::cerr << "[ProfileManager] Cannot write " << path << std::endl;
        return;
    }
    file << overrides.dump(2) << std::endl;
    std::cout << "[ProfileManager] Saved user overrides: " << path << std::endl;
}

// ── Helpers ────────────────────────────────────────────────────────────

std::string ProfileManager::userConfigDir()
{
#if defined(_WIN32)
    auto* appData = std::getenv("APPDATA");
    return (appData ? std::string(appData) : std::string("."))
           + "/beamlab";
#else
    auto* home = std::getenv("XDG_CONFIG_HOME");
    if (home && home[0] != '\0') {
        return std::string(home) + "/beamlab";
    }
    home = std::getenv("HOME");
    return (home ? std::string(home) + "/.config/beamlab" : std::string(".config/beamlab"));
#endif
}

std::string ProfileManager::profilePath(const std::string& name) const
{
    return profilesDir_ + "/" + name + ".json";
}

} // namespace beamlab::core
