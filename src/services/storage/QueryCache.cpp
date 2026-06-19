#include "services/storage/QueryCache.h"

namespace beamlab::services::storage {

QueryCache::QueryCache(std::size_t maxEntries)
    : maxEntries_(maxEntries > 0 ? maxEntries : 1000)
{
}

void QueryCache::invalidateByPrefix(const std::string& prefix)
{
    std::unique_lock<std::shared_mutex> lock(mutex_);

    auto it = index_.begin();
    while (it != index_.end()) {
        if (it->first.compare(0, prefix.size(), prefix) == 0) {
            lruList_.erase(it->second);
            it = index_.erase(it);
        } else {
            ++it;
        }
    }
}

void QueryCache::invalidateAll()
{
    std::unique_lock<std::shared_mutex> lock(mutex_);
    lruList_.clear();
    index_.clear();
}

QueryCache::Stats QueryCache::stats() const
{
    auto h = hits_.load();
    auto m = misses_.load();
    auto total = h + m;

    Stats s{};
    s.hits   = h;
    s.misses = m;
    s.hitRate = (total > 0) ? static_cast<double>(h) / static_cast<double>(total) : 0.0;
    s.currentEntries = index_.size();
    return s;
}

} // namespace beamlab::services::storage
