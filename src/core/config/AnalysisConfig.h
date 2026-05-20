#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace beamlab::core {

struct StorageConfig {
    int64_t sqlite_threshold_mb{100};
    int64_t batch_size{100000};
    bool db_in_memory{false};
};

struct ImportConfig {
    int max_preview_lines{12};
    bool strict_header{true};
    std::vector<std::string> expected_columns{
        "x_cm", "y_cm", "z_cm", "edep_MeV", "kinE_MeV",
        "momx_MeV", "momy_MeV", "momz_MeV", "time_ns",
        "trackID", "parentID", "eventID", "pdg",
        "particleName", "source_file"
    };
};

struct BinningConfig {
    std::string default_mode{"uniform"};
    int axial_bins{501};
    int half_window_frames{5};
};

struct FocusConfig {
    std::string metric{"transverse_rms_radius"};
    bool smooth_curve{false};
};

struct EnvelopeConfig {
    int proxy_resample_points{96};
    double proxy_preview_scale{1.0};
    int angular_sectors{128};
    double quantile{0.90};
};

struct SurfacesConfig {
    int lens_boundary_points{128};
    int lens_radial_layers{16};
    double lens_center_thickness_m{0.0035};
    double lens_edge_thickness_m{0.0006};
};

struct AnalysisParams {
    std::string axis_mode{"auto"};
    std::string reference_mode{"auto"};
    BinningConfig binning{};
    FocusConfig focus{};
    EnvelopeConfig envelope{};
    SurfacesConfig surfaces{};
};

struct PreviewConfig {
    int trajectories{10000};
    int samples_per_trajectory{200};
    int slice_points{10000};
};

struct Mp4ExportConfig {
    int frame_count{120};
    int framerate{30};
};

struct PngExportConfig {
    int plot_width{1900};
    int plot_height{1300};
};

struct ExportConfig {
    Mp4ExportConfig mp4{};
    PngExportConfig png{};
};

struct UiConfig {
    int log_max_lines{10000};
    int progress_update_interval_ms{100};
};

struct PipelineConfig {
    bool use_native{false};
};

struct AnalysisRunConfig {
    StorageConfig storage{};
    ImportConfig import{};
    AnalysisParams analysis{};
    PreviewConfig preview{};
    ExportConfig export_{};
    UiConfig ui{};
    PipelineConfig pipeline{};
};

} // namespace beamlab::core
