#pragma once

#include "core/config/AnalysisConfig.h"
#include "core/pipeline/ProgressTracker.h"

#include <memory>
#include <string>

namespace beamlab::core {

class ISampleStorage;

struct PipelineResult {
    bool success{false};
    std::string outputDir;
    std::string manifestPath;
    std::string errorMessage;
};

class AnalysisPipeline {
public:
    explicit AnalysisPipeline(const AnalysisRunConfig& config);

    PipelineResult run(const std::string& csvPath,
                       const std::string& outputDir,
                       ProgressTracker& progress);

private:
    AnalysisRunConfig config_;
};

} // namespace beamlab::core
