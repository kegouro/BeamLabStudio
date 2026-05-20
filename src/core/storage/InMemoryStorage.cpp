#include "core/storage/InMemoryStorage.h"
#include "core/storage/SqliteStorage.h"

#include "data/model/TrajectorySample.h"

#include <algorithm>
#include <stdexcept>

namespace beamlab::core {

void InMemoryStorage::beginTrajectory(const std::string& trajectoryId)
{
    currentTrajectoryId_ = trajectoryId;
}

void InMemoryStorage::addSample(const beamlab::data::TrajectorySample& sample)
{
    allSamples_.push_back(sample);
    trajIndex_[currentTrajectoryId_].push_back(allSamples_.size() - 1);
}

void InMemoryStorage::endTrajectory()
{
    currentTrajectoryId_.clear();
}

void InMemoryStorage::flush()
{
    // No-op: all data already in memory
}

uint64_t InMemoryStorage::totalSampleCount() const
{
    return allSamples_.size();
}

uint64_t InMemoryStorage::trajectoryCount() const
{
    return trajIndex_.size();
}

std::vector<std::string> InMemoryStorage::trajectoryIds() const
{
    std::vector<std::string> ids;
    ids.reserve(trajIndex_.size());
    for (const auto& [id, _] : trajIndex_) {
        ids.push_back(id);
    }
    return ids;
}

std::vector<beamlab::data::TrajectorySample> InMemoryStorage::getSamplesByTrajectory(
    const std::string& trajectoryId) const
{
    auto it = trajIndex_.find(trajectoryId);
    if (it == trajIndex_.end()) return {};

    std::vector<beamlab::data::TrajectorySample> result;
    result.reserve(it->second.size());
    for (auto idx : it->second) {
        result.push_back(allSamples_[idx]);
    }
    return result;
}

std::vector<beamlab::data::TrajectorySample> InMemoryStorage::getBatch(
    uint64_t offset, uint64_t count) const
{
    if (offset >= allSamples_.size()) return {};
    const auto end = std::min(offset + count, allSamples_.size());
    return {allSamples_.begin() + static_cast<int64_t>(offset),
            allSamples_.begin() + static_cast<int64_t>(end)};
}

std::vector<beamlab::data::TrajectorySample> InMemoryStorage::getAxialRange(
    double zMin, double zMax) const
{
    std::vector<beamlab::data::TrajectorySample> result;
    for (const auto& s : allSamples_) {
        if (s.position_m.z >= zMin && s.position_m.z < zMax) {
            result.push_back(s);
        }
    }
    return result;
}

std::unique_ptr<ISampleStorage> ISampleStorage::create(uint64_t estimatedFileSize)
{
    constexpr uint64_t kThreshold = 100ULL * 1024 * 1024; // 100 MB
    if (estimatedFileSize < kThreshold) {
        return std::make_unique<InMemoryStorage>();
    }
    return std::make_unique<SqliteStorage>(":memory:");
}

} // namespace beamlab::core
