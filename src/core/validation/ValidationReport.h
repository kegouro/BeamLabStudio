#pragma once

#include "core/validation/ValidationIssue.h"

#include <vector>

namespace beamlab::core {

struct ValidationReport {
    std::vector<ValidationIssue> issues{};

    [[nodiscard]] bool hasErrors() const
    {
        for (const auto& issue : issues) {
            if (issue.severity == ValidationSeverity::Error) {
                return true;
            }
        }
        return false;
    }
};

} // namespace beamlab::core
