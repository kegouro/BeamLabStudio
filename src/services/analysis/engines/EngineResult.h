#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace beamlab::services::analysis {

/// Result produced by a single analysis engine run.
struct EngineResult {
    /// True when the engine completed successfully.
    bool success{false};

    /// Human-readable error message when success is false.
    std::optional<std::string> error;

    /// Arbitrary JSON metrics emitted by the engine (e.g. number of
    /// frames processed, timing breakdown, parameter values used).
    /// Empty object by default.
    nlohmann::json metrics = nlohmann::json::object();

    /// Paths to files the engine wrote to disk during execution.
    /// For engines that produce no files (e.g. in-memory statistics),
    /// this vector remains empty.
    std::vector<std::filesystem::path> outputFiles;

    static EngineResult ok(nlohmann::json m = nlohmann::json::object(),
                           std::vector<std::filesystem::path> files = {})
    {
        return {true, std::nullopt, std::move(m), std::move(files)};
    }

    static EngineResult fail(std::string msg,
                             nlohmann::json m = nlohmann::json::object())
    {
        return {false, std::move(msg), std::move(m), {}};
    }
};

} // namespace beamlab::services::analysis
