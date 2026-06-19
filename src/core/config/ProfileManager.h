#pragma once

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace beamlab::core {

class ProfileManager {
public:
    explicit ProfileManager(std::string profilesDir = "config/profiles",
                            std::string defaultPath = "config/default_analysis.json");

    // Load a single profile file (no merge).
    nlohmann::json loadProfile(const std::string& name) const;

    // List available profiles (filenames without .json extension).
    std::vector<std::string> availableProfiles() const;

    // Full resolution chain:
    //   default.json  →  profile.json  →  user overrides  →  CLI overrides
    // Each layer deep-merges into the previous (nested objects combined).
    nlohmann::json resolve(const std::string& profileName,
                           const nlohmann::json& cliOverrides = {}) const;

    // Save user overrides to ~/.config/beamlab/<name>.json.
    void saveUserOverrides(const std::string& profileName,
                           const nlohmann::json& overrides) const;

    // —— Config path helpers ——

    static std::string userConfigDir();
    std::string profilePath(const std::string& name) const;

private:
    std::string profilesDir_;
    std::string defaultPath_;

    static nlohmann::json deepMerge(const nlohmann::json& base,
                                    const nlohmann::json& override_);
    static nlohmann::json readJsonFile(const std::string& path);
};

} // namespace beamlab::core
