#pragma once

#include <cstddef>
#include <string>

namespace beamlab::app {

struct CommandLineOptions {
    bool show_help{false};

    std::string input_file{};
    std::string output_directory{"outputs/latest_run"};

    std::string axis_mode{"auto"};
    std::string reference_mode{"auto"};
    std::string binning_mode{"uniform"};

    std::size_t axial_bin_count{501};
    std::size_t half_window_frames{5};

    std::size_t caustic_resample_points{96};
    double caustic_preview_scale{50.0};

    std::size_t lens_boundary_points{128};
    std::size_t lens_radial_layers{16};
    double lens_center_thickness_m{0.003};
    double lens_edge_thickness_m{0.0005};
    double lens_preview_scale{80.0};

    std::size_t preview_trajectory_count{300};
    std::size_t preview_samples_per_trajectory{300};
    std::size_t preview_slice_point_count{20000};
};

class CommandLineParser {
public:
    [[nodiscard]] static CommandLineOptions parse(int argc, char** argv);
    static void printHelp(const std::string& executable_name);
};

} // namespace beamlab::app
