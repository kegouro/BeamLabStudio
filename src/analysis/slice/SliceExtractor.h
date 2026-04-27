#pragma once

#include "data/model/BeamSlice.h"
#include "data/model/FocusCurve.h"
#include "data/model/TrajectoryDataset.h"

#include <cstddef>

namespace beamlab::analysis {

class SliceExtractor {
public:
    [[nodiscard]] beamlab::data::BeamSlice extractFrameSlice(const beamlab::data::TrajectoryDataset& dataset,
                                                            const beamlab::data::FocusCurve& curve,
                                                            std::size_t frame_index) const;
};

} // namespace beamlab::analysis
