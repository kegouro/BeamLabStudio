#pragma once

#include <cstddef>

namespace beamlab::analysis {

struct FrameStatistics {
    double reference_value{0.0};
    double reference_min_value{0.0};
    double reference_max_value{0.0};
    std::size_t point_count{0};

    double centroid_u{0.0};
    double centroid_v{0.0};

    double sigma_u{0.0};
    double sigma_v{0.0};

    double r_rms{0.0};
};

} // namespace beamlab::analysis
