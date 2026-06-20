#pragma once

#include "services/analysis/engines/IAnalysisEngine.h"

#include <atomic>
#include <cstdint>
#include <memory>

namespace beamlab::services::analysis {

/// Streaming frame-statistics engine.
///
/// Reads batches from IStorageBackend instead of loading the full
/// TrajectoryDataset into memory, making it suitable for datasets
/// larger than available RAM.
///
/// ## Config JSON keys
///
/// | Key | Type | Default | Description |
/// |-----|------|---------|-------------|
/// | `axis_frame.origin` | `{x,y,z}` | `{0,0,0}` | Frame origin |
/// | `axis_frame.longitudinal` | `{x,y,z}` | `{0,0,1}` | Longitudinal axis |
/// | `axis_frame.transverse_u` | `{x,y,z}` | `{1,0,0}` | Transverse U axis |
/// | `axis_frame.transverse_v` | `{x,y,z}` | `{0,1,0}` | Transverse V axis |
/// | `bin_count` | integer | `501` | Number of axial bins |
/// | `batch_size` | integer | `100000` | Samples per read batch |
///
/// ## Two-pass algorithm
///
/// ```
/// Pass 1 — scan all batches to find global s_min/s_max.
/// Pass 2 — re-scan all batches, project → bin into accumulators.
/// ```
///
/// Memory: O(bin_count) regardless of dataset size.
///
class FrameStatisticsPlugin final : public IAnalysisEngine {
public:
    FrameStatisticsPlugin();
    ~FrameStatisticsPlugin() override = default;

    // ── Identity ────────────────────────────────────────────────────

    std::string name() const override { return "FrameStatistics"; }
    std::string version() const override { return "2.0.0"; }

    // ── Capabilities ────────────────────────────────────────────────

    bool requiresBinnedData() const override { return false; }
    bool requiresFullScan()   const override { return true; }

    std::size_t estimatedMemoryBytes(std::uint64_t /*sampleCount*/) const override
    {
        // One accumulator per bin (≈120 bytes each) + a small read buffer.
        return maxBins_ * 128 + 4 * 1024 * 1024;
    }

    // ── Execution ───────────────────────────────────────────────────

    EngineResult execute(
        const beamlab::services::storage::IStorageBackend& storage,
        const nlohmann::json& config,
        ProgressCallback onProgress) override;

    // ── Validation ──────────────────────────────────────────────────

    std::vector<std::string> validateConfig(
        const nlohmann::json& config) const override;

    // ── Cancellation ───────────────────────────────────────────────

    void cancel() { cancelled_.store(true); }
    bool isCancelled() const { return cancelled_.load(); }

private:
    static constexpr std::size_t maxBins_ = 10000;
    std::atomic<bool> cancelled_{false};
};

} // namespace beamlab::services::analysis
