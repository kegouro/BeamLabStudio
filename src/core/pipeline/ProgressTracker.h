#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>

namespace beamlab::core {

struct AnalysisProgress {
    double fraction{0.0};          // 0.0 to 1.0
    std::string stage;              // "importing", "analyzing", "exporting"
    int64_t bytesProcessed{0};
    int64_t totalBytes{0};
};

class ProgressTracker {
public:
    virtual ~ProgressTracker() = default;

    virtual void onProgress(const AnalysisProgress& progress) = 0;
    virtual void onComplete(bool success, const std::string& message) = 0;
    virtual void onLogLine(const std::string& line) = 0;

    bool cancelled() const { return cancelled_.load(std::memory_order_relaxed); }
    void cancel() { cancelled_.store(true, std::memory_order_relaxed); }

private:
    std::atomic<bool> cancelled_{false};
};

} // namespace beamlab::core
