#pragma once

#include "data/ids/TrajectoryId.h"
#include "data/model/TrajectorySample.h"
#include "data/model/AxisFrame.h"
#include "analysis/statistics/FrameStatistics.h"
#include "core/config/AnalysisConfig.h"  // BinningConfig

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace beamlab::data {

struct TrajectoryMetadata {
    TrajectoryId id;
    std::string particleName;
    int pdgCode{0};
};

struct TrajectorySummary {
    TrajectoryId id;
    std::uint64_t sampleCount{0};
    double minZ_m{0.0};
    double maxZ_m{0.0};
    double totalEdep_MeV{0.0};
};

} // namespace beamlab::data

namespace beamlab::data::repositories {

/// Abstract data-access interface for trajectory data.
///
/// Hides the storage backend (InMemory / SQLite / future Parquet)
/// behind a uniform API.  Implementations may add caching.
class ITrajectoryRepository {
public:
    virtual ~ITrajectoryRepository() = default;

    /// All trajectory IDs stored in the repository.
    [[nodiscard]] virtual std::vector<TrajectoryId> getAllTrajectoryIds() const = 0;

    /// Lightweight summary for a single trajectory.
    [[nodiscard]] virtual std::optional<TrajectorySummary> getSummary(
        TrajectoryId id) const = 0;

    /// All samples for a trajectory (may be large — use sparingly).
    [[nodiscard]] virtual std::vector<TrajectorySample> getSamples(
        TrajectoryId id) const = 0;

    /// Aggregated frame statistics for an axis frame and binning config.
    /// Implementations SHOULD cache this result.
    [[nodiscard]] virtual std::vector<analysis::FrameStatistics> computeFrameStats(
        const AxisFrame& axis,
        const beamlab::core::BinningConfig& binning) const = 0;

    // ── Mutations ──────────────────────────────────────────────────

    virtual void insertTrajectory(TrajectoryId id,
                                   const TrajectoryMetadata& meta) = 0;
    virtual void insertSample(const TrajectorySample& sample) = 0;
};

} // namespace beamlab::data::repositories
