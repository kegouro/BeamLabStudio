#pragma once

#include "core/config/AnalysisConfig.h"

#include <string>

namespace beamlab::core {

class ConfigLoader {
public:
    static AnalysisRunConfig loadDefaults();
    static AnalysisRunConfig loadFromFile(const std::string& path);
    static AnalysisRunConfig merge(const AnalysisRunConfig& base,
                                   const AnalysisRunConfig& override);
};

} // namespace beamlab::core
