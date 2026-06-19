#pragma once

#include "core/storage/ISampleStorage.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace beamlab::data {
struct TrajectorySample;
} // namespace beamlab::data

namespace beamlab::core {

class InMemoryStorage final : public ISampleStorage {
public:
    void beginTrajectory(const std::string& trajectoryId) override;
    void addSample(const beamlab::data::TrajectorySample& sample) override;
    void endTrajectory() override;
    void flush() override;

    uint64_t totalSampleCount() const override;
    uint64_t trajectoryCount() const override;
    std::vector<std::string> trajectoryIds() const override;

    std::vector<beamlab::data::TrajectorySample> getSamplesByTrajectory(
        const std::string& trajectoryId) const override;

    std::vector<beamlab::data::TrajectorySample> getBatch(
        uint64_t offset, uint64_t count) const override;

    std::vector<beamlab::data::TrajectorySample> getAxialRange(
        double zMin, double zMax) const override;

    std::pair<double, double> getZRange() const override;

private:
    std::string currentTrajectoryId_;
    std::vector<beamlab::data::TrajectorySample> allSamples_;
    std::unordered_map<std::string, std::vector<uint64_t>> trajIndex_;
};

} // namespace beamlab::core
