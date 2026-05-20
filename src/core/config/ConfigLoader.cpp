#include "core/config/ConfigLoader.h"

#include <nlohmann/json.hpp>

#include <fstream>

namespace beamlab::core {

AnalysisRunConfig ConfigLoader::loadDefaults()
{
    return AnalysisRunConfig{};
}

AnalysisRunConfig ConfigLoader::loadFromFile(const std::string& path)
{
    AnalysisRunConfig config{};
    std::ifstream f(path);
    if (!f) return config;
    try {
        auto j = nlohmann::json::parse(f);
        if (j.contains("storage")) {
            auto& s = j["storage"];
            if (s.contains("sqlite_threshold_mb")) config.storage.sqlite_threshold_mb = s["sqlite_threshold_mb"];
            if (s.contains("batch_size")) config.storage.batch_size = s["batch_size"];
            if (s.contains("db_in_memory")) config.storage.db_in_memory = s["db_in_memory"];
        }
        if (j.contains("preview")) {
            auto& p = j["preview"];
            if (p.contains("trajectories")) config.preview.trajectories = p["trajectories"];
            if (p.contains("samples_per_trajectory")) config.preview.samples_per_trajectory = p["samples_per_trajectory"];
            if (p.contains("slice_points")) config.preview.slice_points = p["slice_points"];
        }
        if (j.contains("analysis")) {
            auto& a = j["analysis"];
            if (a.contains("axis_mode")) config.analysis.axis_mode = a["axis_mode"];
            if (a.contains("reference_mode")) config.analysis.reference_mode = a["reference_mode"];
            if (a.contains("binning")) {
                auto& b = a["binning"];
                if (b.contains("default_mode")) config.analysis.binning.default_mode = b["default_mode"];
                if (b.contains("axial_bins")) config.analysis.binning.axial_bins = b["axial_bins"];
                if (b.contains("half_window_frames")) config.analysis.binning.half_window_frames = b["half_window_frames"];
            }
        }
        if (j.contains("pipeline")) {
            if (j["pipeline"].contains("use_native")) config.pipeline.use_native = j["pipeline"]["use_native"];
        }
        if (j.contains("ui")) {
            if (j["ui"].contains("log_max_lines")) config.ui.log_max_lines = j["ui"]["log_max_lines"];
        }
    } catch (...) {
        return AnalysisRunConfig{};
    }
    return config;
}

AnalysisRunConfig ConfigLoader::merge(const AnalysisRunConfig& base,
                                      const AnalysisRunConfig& override)
{
    auto result = base;
    if (override.storage.sqlite_threshold_mb != 100) result.storage = override.storage;
    if (!override.analysis.axis_mode.empty()) result.analysis.axis_mode = override.analysis.axis_mode;
    if (override.preview.trajectories != 10000) result.preview = override.preview;
    if (override.pipeline.use_native) result.pipeline.use_native = true;
    return result;
}

} // namespace beamlab::core
