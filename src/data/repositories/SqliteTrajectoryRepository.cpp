#include "data/repositories/SqliteTrajectoryRepository.h"

#include <cmath>
#include <sstream>

namespace beamlab::data::repositories {

// ── Constructor ─────────────────────────────────────────────────────

SqliteTrajectoryRepository::SqliteTrajectoryRepository(
    std::shared_ptr<services::storage::SqliteBackend> backend,
    std::shared_ptr<services::storage::QueryCache> cache)
    : backend_(std::move(backend))
    , cache_(std::move(cache))
{
}

// ── Queries ────────────────────────────────────────────────────────

std::vector<TrajectoryId> SqliteTrajectoryRepository::getAllTrajectoryIds() const
{
    std::vector<TrajectoryId> ids;
    auto total = backend_->totalTrajectories();
    ids.reserve(total);

    // Iterate traj IDs via storage batches.
    for (uint64_t i = 0; i < total; ++i) {
        ids.push_back(TrajectoryId(i + 1));
    }
    return ids;
}

std::optional<TrajectorySummary>
SqliteTrajectoryRepository::getSummary(TrajectoryId id) const
{
    auto batch = backend_->readTrajectory(id);
    if (batch.data == nullptr || batch.count == 0)
        return std::nullopt;

    TrajectorySummary s;
    s.id = id;
    s.sampleCount = batch.count;
    s.minZ_m = batch.data[0].position_m.z;
    s.maxZ_m = batch.data[0].position_m.z;

    for (uint64_t i = 0; i < batch.count; ++i) {
        double z = batch.data[i].position_m.z;
        s.minZ_m = std::min(s.minZ_m, z);
        s.maxZ_m = std::max(s.maxZ_m, z);
        s.totalEdep_MeV += batch.data[i].edep_MeV;
    }
    return s;
}

std::vector<TrajectorySample>
SqliteTrajectoryRepository::getSamples(TrajectoryId id) const
{
    auto batch = backend_->readTrajectory(id);
    std::vector<TrajectorySample> result;
    result.reserve(batch.count);
    for (uint64_t i = 0; i < batch.count; ++i)
        result.push_back(batch.data[i]);
    return result;
}

// ── computeFrameStats (with cache) ──────────────────────────────────

std::vector<analysis::FrameStatistics>
SqliteTrajectoryRepository::computeFrameStats(
    const AxisFrame& axis,
    const beamlab::core::BinningConfig& binning) const
{
    auto key = buildCacheKey(axis, binning);

    // ── Cache hit ──────────────────────────────────────────────────
    if (cache_) {
        auto cached = cache_->get<std::vector<analysis::FrameStatistics>>(key);
        if (cached.has_value()) {
            return *cached;
        }
    }

    // ── Cache miss — compute from storage ──────────────────────────
    std::vector<analysis::FrameStatistics> result;

    // Read global z-range from backend.
    auto stats = backend_->globalStats();
    if (stats.count == 0) return result;

    auto nBins = static_cast<std::size_t>(binning.axial_bins);
    double binWidth = (stats.zMax - stats.zMin) / static_cast<double>(nBins);

    result.reserve(nBins);

    for (std::size_t b = 0; b < nBins; ++b) {
        double zMin = stats.zMin + static_cast<double>(b) * binWidth;
        double zMax = zMin + binWidth;

        auto batch = backend_->readAxialRange(zMin, zMax);
        if (batch.count < 3) continue;

        // Compute simple frame stats from the batch.
        analysis::FrameStatistics fs;
        fs.valid = true;
        fs.reference_min_value = zMin;
        fs.reference_max_value = zMax;
        fs.reference_value = (zMin + zMax) * 0.5;
        fs.point_count = static_cast<std::size_t>(batch.count);

        double sumU = 0.0, sumV = 0.0, sumU2 = 0.0, sumV2 = 0.0;
        for (uint64_t i = 0; i < batch.count; ++i) {
            const auto& s = batch.data[i];
            // Project to transverse coordinates (simplified).
            double u = s.position_m.x;
            double v = s.position_m.y;
            sumU += u; sumV += v;
            sumU2 += u * u; sumV2 += v * v;
        }
        auto invN = 1.0 / static_cast<double>(batch.count);
        fs.centroid_u = sumU * invN;
        fs.centroid_v = sumV * invN;
        fs.sigma_u = std::sqrt(std::max(0.0, sumU2 * invN - fs.centroid_u * fs.centroid_u));
        fs.sigma_v = std::sqrt(std::max(0.0, sumV2 * invN - fs.centroid_v * fs.centroid_v));
        fs.r_rms = std::sqrt(fs.sigma_u * fs.sigma_u + fs.sigma_v * fs.sigma_v);

        result.push_back(fs);
    }

    // ── Store in cache ─────────────────────────────────────────────
    if (cache_) {
        cache_->put(key, result, std::chrono::minutes(5));
    }

    return result;
}

// ── Mutations ──────────────────────────────────────────────────────

void SqliteTrajectoryRepository::insertTrajectory(
    TrajectoryId id, const TrajectoryMetadata& meta)
{
    static_cast<void>(id);
    static_cast<void>(meta);
}

void SqliteTrajectoryRepository::insertSample(
    const TrajectorySample& sample)
{
    backend_->beginWriteBatch();
    backend_->writeSample(sample);
    backend_->endWriteBatch();

    // Invalidate frame-stats cache.
    if (cache_) {
        cache_->invalidateByPrefix("framestats/");
        cache_->invalidateByPrefix("global/");
    }
}

// ── Cache key builder ──────────────────────────────────────────────

std::string SqliteTrajectoryRepository::buildCacheKey(
    const AxisFrame& axis, const beamlab::core::BinningConfig& binning) const
{
    std::stringstream ss;
    // Framestats key: "framestats/{zMin:.2f}/{zMax:.2f}/{nBins}"
    // Use axis confidence as a discriminator.
    ss << "framestats/"
       << "bins=" << binning.axial_bins
       << "/method=" << axis.derivation_method
       << "/conf=" << axis.confidence;
    return ss.str();
}

} // namespace beamlab::data::repositories
