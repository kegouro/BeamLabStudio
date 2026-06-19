#pragma once

#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <functional>
#include <list>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace beamlab::services::storage {

/// LRU cache with TTL for storage query results.
///
/// Keys follow the convention:
///   framestats/{datasetId}/{axialMin}/{axialMax}/{nBins}
///   global/{datasetId}
///   trajectory/{datasetId}/{trajId}
///
/// Values are serialised to msgpack/cbor/JSON via nlohmann::json for
/// type-erased storage.
class QueryCache {
public:
    struct Stats {
        std::size_t hits{0};
        std::size_t misses{0};
        double hitRate{0.0};
        std::size_t currentEntries{0};
    };

    explicit QueryCache(std::size_t maxEntries = 1000);

    // ── Access ────────────────────────────────────────────────────

    template<typename T>
    std::optional<T> get(const std::string& key)
    {
        std::shared_lock<std::shared_mutex> lock(mutex_);

        auto it = index_.find(key);
        if (it == index_.end()) {
            ++misses_;
            return std::nullopt;
        }

        auto listIt = it->second;

        // ── TTL check ──────────────────────────────────────────────
        if (std::chrono::steady_clock::now() > listIt->expiresAt) {
            // Expired — remove and report miss.
            // Convert shared lock to unique lock for eviction.
            lock.unlock();
            {
                std::unique_lock<std::shared_mutex> ul(mutex_);
                auto it2 = index_.find(key);
                if (it2 != index_.end()) {
                    lruList_.erase(it2->second);
                    index_.erase(it2);
                }
            }
            ++misses_;
            return std::nullopt;
        }

        // Deserialise.
        try {
            auto json = nlohmann::json::from_msgpack(
                listIt->serialized.begin(), listIt->serialized.end());
            T value = json.get<T>();
            ++hits_;
            return value;
        } catch (const std::exception&) {
            ++misses_;
            return std::nullopt;
        }
    }

    template<typename T>
    void put(const std::string& key,
             const T& value,
             std::chrono::seconds ttl = std::chrono::minutes(5))
    {
        std::unique_lock<std::shared_mutex> lock(mutex_);

        // Serialise value to msgpack.
        auto json = nlohmann::json(value);
        auto packed = nlohmann::json::to_msgpack(json);

        auto it = index_.find(key);
        if (it != index_.end()) {
            // Update existing entry.
            auto listIt = it->second;
            listIt->serialized = std::move(packed);
            listIt->expiresAt = std::chrono::steady_clock::now() + ttl;
            // Move to front (most recently used).
            lruList_.splice(lruList_.begin(), lruList_, listIt);
            return;
        }

        // Evict if at capacity.
        while (lruList_.size() >= maxEntries_) {
            const auto& oldest = lruList_.back();
            index_.erase(oldest.key);
            lruList_.pop_back();
        }

        CacheEntry entry;
        entry.key          = key;
        entry.serialized   = std::move(packed);
        entry.expiresAt    = std::chrono::steady_clock::now() + ttl;

        lruList_.emplace_front(std::move(entry));
        index_[key] = lruList_.begin();
    }

    // ── Invalidation ──────────────────────────────────────────────

    void invalidateByPrefix(const std::string& prefix);
    void invalidateAll();

    // ── Statistics ────────────────────────────────────────────────

    Stats stats() const;

    std::size_t size() const { return index_.size(); }
    std::size_t maxSize() const { return maxEntries_; }

private:
    struct CacheEntry {
        std::string key;
        std::vector<std::uint8_t> serialized;
        std::chrono::steady_clock::time_point expiresAt;
    };

    std::list<CacheEntry> lruList_;
    std::unordered_map<std::string, decltype(lruList_)::iterator> index_;
    std::size_t maxEntries_;

    mutable std::shared_mutex mutex_;
    std::atomic<std::size_t> hits_{0};
    std::atomic<std::size_t> misses_{0};
};

} // namespace beamlab::services::storage
