#pragma once

#include "analysis/focus/FocusParameters.h"
#include "data/model/FocusCurve.h"
#include "data/model/FocusResult.h"

namespace beamlab::analysis {

class FocusDetector {
public:
    [[nodiscard]] beamlab::data::FocusResult detect(const beamlab::data::FocusCurve& curve,
                                                    const FocusParameters& parameters) const;
};

} // namespace beamlab::analysis
