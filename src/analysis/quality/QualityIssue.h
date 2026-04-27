#pragma once

#include "analysis/quality/QualitySeverity.h"

#include <string>

namespace beamlab::analysis {

struct QualityIssue {
    QualitySeverity severity{QualitySeverity::Info};
    std::string code{};
    std::string title{};
    std::string detail{};
    std::string suggestion{};
};

} // namespace beamlab::analysis
