#pragma once

#include "data/repositories/ITrajectoryRepository.h"
#include "services/storage/SqliteBackend.h"
#include "services/storage/QueryCache.h"

#include <memory>
#include <string>

namespace beamlab::data::repositories {

/// SQLite-backed repository with optional QueryCache.
///
/// If a cache is provided (non-null), `computeFrameStats()` results
/// are cached keyed by axis frame + binning parameters.  Mutations
/// (`insertSample`, `insertTrajectory`) invalidate all frame-stat
/// caches automatically.
class SqliteTrajectoryRepository final : public ITrajectoryRepository {
public:
    explicit SqliteTrajectoryRepository(
        std::shared_ptr<services::storage::SqliteBackend> backend,
        std::shared_ptr<services::storage::QueryCache> cache = nullptr);

    // ── Queries ────────────────────────────────────────────────────

    [[nodiscard]] std::vector<TrajectoryId> getAllTrajectoryIds() const override;

    [[nodiscard]] std::optional<TrajectorySummary> getSummary(
        TrajectoryId id) const override;

    [[nodiscard]] std::vector<TrajectorySample> getSamples(
        TrajectoryId id) const override;

    [[nodiscard]] std::vector<analysis::FrameStatistics> computeFrameStats(
        const AxisFrame& axis,
        const beamlab::core::BinningConfig& binning) const override;

    // ── Mutations ──────────────────────────────────────────────────

    void insertTrajectory(TrajectoryId id,
                           const TrajectoryMetadata& meta) override;
    void insertSample(const TrajectorySample& sample) override;

private:
    std::shared_ptr<services::storage::SqliteBackend> backend_;
    std::shared_ptr<services::storage::QueryCache> cache_;

    [[nodiscard]] std::string buildCacheKey(
        const AxisFrame& axis,
        const beamlab::core::BinningConfig& binning) const;
};

} // namespace beamlab::data::repositories
