#include "analysis/slice/SlicePlanner.h"

#include <algorithm>

namespace beamlab::analysis {

std::vector<std::size_t> SlicePlanner::planFrameSlices(const beamlab::data::FocusResult& focus,
                                                       const SliceParameters& parameters) const
{
    std::vector<std::size_t> indices{};

    if (!focus.valid || focus.curve.points.empty()) {
        return indices;
    }

    const std::size_t total = focus.curve.points.size();
    const std::size_t focus_index = focus.focus_index;

    const std::size_t begin = focus_index > parameters.half_window_frames
        ? focus_index - parameters.half_window_frames
        : 0;

    const std::size_t end = std::min(total - 1, focus_index + parameters.half_window_frames);

    for (std::size_t i = begin; i <= end; ++i) {
        indices.push_back(i);
    }

    if (parameters.max_slices > 0 && indices.size() > parameters.max_slices) {
        const std::size_t excess = indices.size() - parameters.max_slices;
        const std::size_t trim_left = excess / 2;
        const std::size_t trim_right = excess - trim_left;

        indices.erase(indices.begin(), indices.begin() + static_cast<std::ptrdiff_t>(trim_left));
        indices.erase(indices.end() - static_cast<std::ptrdiff_t>(trim_right), indices.end());
    }

    return indices;
}

} // namespace beamlab::analysis
