#include "data/model/TrajectorySample.h"
#include "services/storage/StorageManager.h"
#include "services/storage/SqliteBackend.h"

#include <algorithm>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;
using namespace std::chrono;

namespace beamlab::services::storage {

// ── InMemoryBackend (minimal inline implementation) ───────────────────

namespace {

class InMemoryBackend final : public IStorageBackend {
public:
    std::string backendName() const override { return "InMemory"; }

    uint64_t totalSamples() const override
    {
        std::lock_guard<std::mutex> lock(mtx_);
        return samples_.size();
    }

    uint64_t totalTrajectories() const override
    {
        std::lock_guard<std::mutex> lock(mtx_);
        if (samples_.empty()) return 0;
        TrajectoryId last{0};
        uint64_t count = 0;
        for (const auto& s : samples_) {
            if (s.trajectory_id != last) {
                ++count;
                last = s.trajectory_id;
            }
        }
        return count;
    }

    void beginWriteBatch() override {}
    void writeSample(const TrajectorySample& sample) override
    {
        std::lock_guard<std::mutex> lock(mtx_);
        samples_.push_back(sample);
    }
    void endWriteBatch() override {}

    SampleBatch readBatch(uint64_t offset, uint64_t count) const override
    {
        std::lock_guard<std::mutex> lock(mtx_);
        if (offset >= samples_.size()) return {nullptr, 0, offset};
        auto avail = std::min<uint64_t>(count, samples_.size() - offset);
        buf_.assign(samples_.begin() + static_cast<ptrdiff_t>(offset),
                    samples_.begin() + static_cast<ptrdiff_t>(offset + avail));
        return {buf_.data(), avail, offset};
    }

    SampleBatch readAxialRange(double zMin, double zMax) const override
    {
        std::lock_guard<std::mutex> lock(mtx_);
        buf_.clear();
        for (const auto& s : samples_) {
            if (s.position_m.z >= zMin && s.position_m.z <= zMax) {
                buf_.push_back(s);
            }
        }
        return {buf_.data(), buf_.size(), 0};
    }

    SampleBatch readTrajectory(TrajectoryId id) const override
    {
        std::lock_guard<std::mutex> lock(mtx_);
        buf_.clear();
        for (const auto& s : samples_) {
            if (s.trajectory_id == id) {
                buf_.push_back(s);
            }
        }
        return {buf_.data(), buf_.size(), 0};
    }

    GlobalStats globalStats() const override
    {
        std::lock_guard<std::mutex> lock(mtx_);
        GlobalStats st;
        st.count = samples_.size();
        if (samples_.empty()) return st;
        st.zMin = st.zMax = samples_.front().position_m.z;
        for (const auto& s : samples_) {
            st.zMin = std::min(st.zMin, s.position_m.z);
            st.zMax = std::max(st.zMax, s.position_m.z);
            st.edepSum += s.edep_MeV;
        }
        return st;
    }

    void vacuum() override {}
    uint64_t diskUsage() const override { return 0; }

private:
    mutable std::mutex mtx_;
    std::vector<TrajectorySample> samples_;
    mutable std::vector<TrajectorySample> buf_;
};

} // anonymous namespace

// ── StorageManager ─────────────────────────────────────────────────────

StorageManager::StorageManager() = default;

std::unique_ptr<IStorageBackend> StorageManager::create(
    uint64_t estimatedFileSizeBytes,
    const beamlab::core::AnalysisRunConfig& config) const
{
    // Force InMemory when explicitly requested.
    if (config.storage.db_in_memory) {
        return std::make_unique<InMemoryBackend>();
    }

    auto thresholdBytes = static_cast<uint64_t>(
        config.storage.sqlite_threshold_mb) * 1024ULL * 1024ULL;

    if (estimatedFileSizeBytes > thresholdBytes) {
        auto tmpPath = (fs::temp_directory_path() /
                        "beamlab_import_XXXXXX.db").string();
        auto now = steady_clock::now().time_since_epoch().count();
        auto pos = tmpPath.find("XXXXXX");
        if (pos != std::string::npos) {
            tmpPath.replace(pos, 6, std::to_string(now));
        }

        auto backend = std::make_unique<SqliteBackend>(tmpPath);
        backend->setBatchSize(
            static_cast<uint64_t>(config.storage.batch_size));
        return backend;
    }

    return std::make_unique<InMemoryBackend>();
}

// ── LRU cache ──────────────────────────────────────────────────────────

bool StorageManager::isCacheEntryExpired(const CacheEntry& entry) const
{
    return (steady_clock::now() - entry.insertedAt) > kCacheTTL;
}

std::optional<SampleBatch> StorageManager::getCachedBatch(
    uint64_t offset, uint64_t count)
{
    std::lock_guard<std::mutex> lock(cacheMutex_);

    auto key = "batch:" + std::to_string(offset) + ":" + std::to_string(count);
    auto it = cacheIndex_.find(key);

    if (it == cacheIndex_.end()) {
        {
            std::lock_guard<std::mutex> ml(metricsMutex_);
            metrics_.cacheMisses++;
        }
        return std::nullopt;
    }

    auto listIt = it->second;
    auto& entry = listIt->second;

    if (isCacheEntryExpired(entry)) {
        cacheList_.erase(listIt);
        cacheIndex_.erase(it);
        {
            std::lock_guard<std::mutex> ml(metricsMutex_);
            metrics_.cacheMisses++;
        }
        return std::nullopt;
    }

    // Move to front (most recently used).
    cacheList_.splice(cacheList_.begin(), cacheList_, listIt);
    {
        std::lock_guard<std::mutex> ml(metricsMutex_);
        metrics_.cacheHits++;
    }

    return SampleBatch{entry.data.data(), entry.data.size(), offset};
}

void StorageManager::cacheBatch(uint64_t offset, uint64_t count,
                                 const SampleBatch& batch)
{
    std::lock_guard<std::mutex> lock(cacheMutex_);

    auto key = "batch:" + std::to_string(offset) + ":" + std::to_string(count);

    auto idxIt = cacheIndex_.find(key);
    if (idxIt != cacheIndex_.end()) {
        auto listIt = idxIt->second;
        listIt->second.data.assign(batch.data, batch.data + batch.count);
        listIt->second.insertedAt = steady_clock::now();
        cacheList_.splice(cacheList_.begin(), cacheList_, listIt);
        return;
    }

    while (cacheList_.size() >= kMaxCacheEntries) {
        auto& oldest = cacheList_.back();
        cacheIndex_.erase(oldest.first);
        cacheList_.pop_back();
    }

    CacheEntry entry;
    entry.data.assign(batch.data, batch.data + batch.count);
    entry.insertedAt = steady_clock::now();

    cacheList_.emplace_front(key, std::move(entry));
    cacheIndex_[key] = cacheList_.begin();
}

void StorageManager::invalidateCache()
{
    std::lock_guard<std::mutex> lock(cacheMutex_);
    cacheList_.clear();
    cacheIndex_.clear();
}

// ── Streaming import ───────────────────────────────────────────────────

void StorageManager::importFrom(
    IStorageImporter& importer,
    const ImportContext& ctx,
    IStorageBackend& backend,
    std::function<void(uint64_t bytesRead, uint64_t totalBytes)> onProgress)
{
    auto notify = [&](uint64_t read, uint64_t total) {
        if (onProgress) onProgress(read, total);
    };

    uint64_t totalBytes = ctx.fileSize;
    if (totalBytes == 0 && !ctx.filePath.empty()) {
        totalBytes = static_cast<uint64_t>(fs::file_size(ctx.filePath));
    }

    notify(0, totalBytes);

    // Speed tracking.
    auto speedStart = steady_clock::now();
    uint64_t lastBytes{0};

    auto updateSpeed = [&](uint64_t currentBytes) {
        lastBytes = currentBytes;
    };

    // Progress throttling: call onProgress at most every ~100ms or 1%.
    auto lastNotifyPct = 0.0;
    auto lastNotifyTime = steady_clock::now();

    auto throttledNotify = [&](uint64_t read, uint64_t total) {
        auto now = steady_clock::now();
        auto denom = total > 0 ? static_cast<double>(total) : 1.0;
        auto pct = static_cast<double>(read) / denom;
        auto elapsedMs = duration_cast<milliseconds>(now - lastNotifyTime).count();

        if (pct - lastNotifyPct >= 0.01 || elapsedMs >= 100 || pct >= 1.0 || read == 0) {
            lastNotifyPct = pct;
            lastNotifyTime = now;
            notify(read, total);
        }
    };

    // RAII guard for write batch.
    struct BatchGuard {
        IStorageBackend& b;
        bool committed = false;
        ~BatchGuard() {
            if (!committed) {
                try { b.endWriteBatch(); } catch (...) {}
            }
        }
    };

    try {
        backend.beginWriteBatch();
        BatchGuard guard{backend};

        importer.import(ctx, backend,
            [&](uint64_t read, uint64_t /*total*/) {
                updateSpeed(read);
                throttledNotify(read, totalBytes);
            });

        notify(totalBytes, totalBytes);
        guard.committed = true;
        backend.endWriteBatch();

    } catch (const std::exception& e) {
        std::cerr << "[StorageManager] Import failed: " << e.what()
                  << std::endl;
        throw;
    }

    // Finalise indices on disk-backed backends.
    if (auto* sqlite = dynamic_cast<SqliteBackend*>(&backend)) {
        sqlite->finalizeIndices();
    }

    // Update metrics.
    auto elapsed = duration<double>(steady_clock::now() - speedStart).count();
    std::lock_guard<std::mutex> ml(metricsMutex_);
    metrics_.totalSamplesImported += backend.totalSamples();
    if (elapsed > 0.0 && lastBytes > 0) {
        metrics_.avgImportSpeedBytesPerSec =
            (metrics_.avgImportSpeedBytesPerSec * 0.7) +
            (static_cast<double>(lastBytes) / elapsed) * 0.3;
    }
}

// ── Metrics ────────────────────────────────────────────────────────────

StorageMetrics StorageManager::metrics() const
{
    std::lock_guard<std::mutex> lock(metricsMutex_);
    return metrics_;
}

} // namespace beamlab::services::storage
