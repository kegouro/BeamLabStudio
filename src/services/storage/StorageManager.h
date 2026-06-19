#pragma once

#include "services/storage/IStorageBackend.h"
#include "core/config/AnalysisConfig.h"

#include <chrono>
#include <cstdint>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

namespace beamlab::services::storage {

// ── Importer interface expected by StorageManager ──────────────────────
// Thin abstraction: importers read a file and write samples into the
// backend.  Adapted from the existing IImporter in src/io/importers/.

struct ImportContext {
    std::string filePath;
    uint64_t fileSize{0};
};

class IStorageImporter {
public:
    virtual ~IStorageImporter() = default;
    virtual std::string name() const = 0;
    virtual void import(const ImportContext& ctx,
                         IStorageBackend& backend,
                         std::function<void(uint64_t, uint64_t)> onProgress) = 0;
};

// ── Storage manager metrics ────────────────────────────────────────────

struct StorageMetrics {
    uint64_t totalSamplesImported{0};
    uint64_t cacheHits{0};
    uint64_t cacheMisses{0};
    double avgImportSpeedBytesPerSec{0.0};
};

// ── StorageManager ─────────────────────────────────────────────────────

class StorageManager {
public:
    StorageManager();

    // ── Factory ────────────────────────────────────────────────────────

    // Decides and creates the optimal backend based on file size and config.
    //   - config.storage.db_in_memory == true  → InMemoryBackend
    //   - fileSize > threshold                 → SqliteBackend
    //   - otherwise                            → InMemoryBackend
    std::unique_ptr<IStorageBackend> create(
        uint64_t estimatedFileSizeBytes,
        const beamlab::core::AnalysisRunConfig& config) const;

    // ── LRU query cache ───────────────────────────────────────────────

    std::optional<SampleBatch> getCachedBatch(uint64_t offset, uint64_t count);
    void cacheBatch(uint64_t offset, uint64_t count, const SampleBatch& batch);
    void invalidateCache();

    // ── Streaming import ───────────────────────────────────────────────

    // Imports a file through the provided importer into the given backend.
    // Handles beginWriteBatch/endWriteBatch lifecycle with RAII.
    // Reports progress via onProgress(bytesRead, totalBytes).
    void importFrom(
        IStorageImporter& importer,
        const ImportContext& ctx,
        IStorageBackend& backend,
        std::function<void(uint64_t bytesRead, uint64_t totalBytes)> onProgress = nullptr);

    // ── Metrics ────────────────────────────────────────────────────────

    StorageMetrics metrics() const;

private:
    // ── LRU cache entry ────────────────────────────────────────────────

    struct CacheEntry {
        std::vector<beamlab::data::TrajectorySample> data;
        std::chrono::steady_clock::time_point insertedAt;
    };

    using CacheList = std::list<std::pair<std::string, CacheEntry>>;
    using CacheIndex = std::unordered_map<std::string, CacheList::iterator>;

    mutable std::mutex cacheMutex_;
    CacheList cacheList_;
    CacheIndex cacheIndex_;
    static constexpr std::size_t kMaxCacheEntries = 100;
    static constexpr auto kCacheTTL = std::chrono::minutes(5);

    bool isCacheEntryExpired(const CacheEntry& entry) const;

    // ── Metrics state ──────────────────────────────────────────────────

    mutable std::mutex metricsMutex_;
    StorageMetrics metrics_;
};

} // namespace beamlab::services::storage
