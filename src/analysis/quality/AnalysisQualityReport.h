#pragma once

#include "analysis/quality/QualityIssue.h"

#include <cstddef>
#include <string>
#include <vector>

namespace beamlab::analysis {

struct AnalysisQualityReport {
    std::string verdict{"PASS"};
    std::vector<QualityIssue> issues{};

    std::size_t info_count{0};
    std::size_t warning_count{0};
    std::size_t error_count{0};
    std::size_t critical_count{0};

    [[nodiscard]] bool passed() const
    {
        return error_count == 0 && critical_count == 0;
    }
};

} // namespace beamlab::analysis
