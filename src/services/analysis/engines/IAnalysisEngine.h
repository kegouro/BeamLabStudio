#pragma once

#include "services/analysis/engines/EngineResult.h"
#include "services/storage/IStorageBackend.h"

#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace beamlab::services::analysis {

/// Signature for real-time progress callbacks.
///
/// \param percent  Completion estimate in range [0.0, 1.0].
/// \param stage    Short human-readable label for the current sub-stage
///                 (e.g. "Binning", "Focus detection", "Writing output").
using ProgressCallback = std::function<void(float percent, std::string_view stage)>;

// ─────────────────────────────────────────────────────────────────────
//  IAnalysisEngine
// ─────────────────────────────────────────────────────────────────────

/// Abstract interface for a pluggable analysis engine.
///
/// Each concrete engine performs one well-defined computation on a
/// storage backend (e.g. frame statistics, focus detection, envelope
/// extraction, quality checking).  Engines are registered in the
/// engine registry and orchestrated by the AnalysisOrchestrator.
///
/// ## Lifecycle during orchestration
///
/// ```
/// 1.  engine = registry.create("FrameStatistics")
/// 2.  engine->validateConfig(config)       ← early rejection
/// 3.  engine->estimatedMemory(sampleCount) ← scheduling decision
/// 4.  engine->execute(storage, config, axis, progress) → result
/// ```
class IAnalysisEngine {
public:
    virtual ~IAnalysisEngine() = default;

    // ── Identity ─────────────────────────────────────────────────

    /// Unique human-readable name, e.g. "FrameStatistics".
    virtual std::string name() const = 0;

    /// Semantic version of this engine's algorithm, e.g. "2.1.0".
    virtual std::string version() const = 0;

    // ── Capabilities ─────────────────────────────────────────────

    /// Returns true when the engine requires data that has already
    /// been binned along the axis frame (e.g. a per-frame histogram).
    /// Engines that work on raw sample positions return false.
    virtual bool requiresBinnedData() const { return false; }

    /// Returns true when the engine must scan every sample in the
    /// dataset (e.g. global statistics).  Engines that can operate
    /// on a random subset or on axial-range slices return false.
    virtual bool requiresFullScan() const { return true; }

    /// Reasonable upper bound on heap memory (bytes) this engine
    /// will allocate to process `sampleCount` samples.
    /// Used by the scheduler to decide how many engines can run in
    /// parallel without exhausting system RAM.
    ///
    /// \param sampleCount  Total samples in the storage backend.
    virtual std::size_t estimatedMemoryBytes(std::uint64_t sampleCount) const = 0;

    // ── Execution ────────────────────────────────────────────────

    /// Run the engine against the open storage backend.
    ///
    /// The engine reads samples via IStorageBackend::readBatch(),
    /// IStorageBackend::readAxialRange(), etc.  It must respect the
    /// cancellation flag when provided.
    ///
    /// \param storage    Backend containing the imported samples.
    /// \param config     Run-time parameters (bin count, thresholds,
    ///                   flags, …) — engine-specific fields are
    ///                   accessed by key.
    /// \param onProgress Optional callback invoked periodically to
    ///                   report real-time progress.
    ///
    /// \return EngineResult with success/failure, optional metrics,
    ///         and paths to any output files written.
    virtual EngineResult execute(
        const beamlab::services::storage::IStorageBackend& storage,
        const nlohmann::json& config,
        ProgressCallback onProgress = nullptr) = 0;

    // ── Validation ───────────────────────────────────────────────

    /// Validate that the provided configuration is acceptable for
    /// this engine.  Called before execute() so that invalid
    /// parameters are caught early without touching the storage.
    ///
    /// \param config  The full analysis configuration as JSON.
    ///
    /// \return Empty vector when the config is valid.  Otherwise a
    ///         list of human-readable error messages, one per issue.
    virtual std::vector<std::string> validateConfig(
        const nlohmann::json& config) const = 0;
};

} // namespace beamlab::services::analysis

// ─────────────────────────────────────────────────────────────────────
//  Example concrete engine
// ─────────────────────────────────────────────────────────────────────
//
// class FrameStatisticsEngine final : public IAnalysisEngine {
// public:
//     std::string name()    const override { return "FrameStatistics"; }
//     std::string version() const override { return "2.1.0"; }
//
//     bool requiresBinnedData() const override { return false; }
//     bool requiresFullScan()   const override { return true; }
//
//     std::size_t estimatedMemoryBytes(std::uint64_t n) const override {
//         return n * sizeof(FrameAccumulator) / 1000 + 16 * 1024 * 1024;
//     }
//
//     EngineResult execute(const IStorageBackend& storage,
//                          const nlohmann::json& cfg,
//                          ProgressCallback onProgress) override
//     {
//         onProgress(0.0f, "Reading samples");
//         auto total = storage.totalSamples();
//         // ... process batches, emit progress each 10% ...
//         for (std::uint64_t off = 0; off < total; off += 100000) {
//             auto batch = storage.readBatch(off, 100000);
//             processBatch(batch);
//             if (onProgress) {
//                 auto pct = static_cast<float>(off) / total;
//                 onProgress(pct, "Computing frame statistics");
//             }
//         }
//         onProgress(1.0f, "Done");
//         return EngineResult::ok({{"frames", 42}, {"mean_sigma", 0.015}});
//     }
//
//     std::vector<std::string> validateConfig(
//         const nlohmann::json& cfg) const override
//     {
//         std::vector<std::string> issues;
//         if (!cfg.contains("axial_bins") || !cfg["axial_bins"].is_number())
//             issues.push_back("'axial_bins' must be a positive integer");
//         return issues;
//     }
// };
