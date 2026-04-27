#pragma once

#include <cstddef>

namespace beamlab::analysis {

struct SurfaceBuildParameters {
    std::size_t resample_point_count{96};
    bool close_start_cap{false};
    bool close_end_cap{false};
};

} // namespace beamlab::analysis
