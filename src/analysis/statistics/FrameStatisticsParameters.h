#pragma once

#include <cstddef>

namespace beamlab::analysis {

enum class ReferenceMode {
    Auto,
    Synchronized,
    AxialBins
};

enum class AxialBinningMode {
    Uniform,
    EqualCount
};

struct FrameStatisticsParameters {
    ReferenceMode reference_mode{ReferenceMode::Auto};
    AxialBinningMode axial_binning_mode{AxialBinningMode::Uniform};
    std::size_t axial_bin_count{501};
};

} // namespace beamlab::analysis
