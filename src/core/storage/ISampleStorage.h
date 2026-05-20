#pragma once

#include <cstdint>
#include <memory>
#include <vector>

namespace beamlab::data {
struct TrajectorySample;
} // namespace beamlab::data

namespace beamlab::core {

struct SampleBatch {
    const beamlab::data::TrajectorySample* data{nullptr};
    uint64_t count{0};
    uint64_t offset{0};
};

class ISampleStorage {
public:
    virtual ~ISampleStorage() = default;

    virtual void beginTrajectory(const std::string& trajectoryId) = 0;
    virtual void addSample(const beamlab::data::TrajectorySample& sample) = 0;
    virtual void endTrajectory() = 0;
    virtual void flush() = 0;

    virtual uint64_t totalSampleCount() const = 0;
    virtual uint64_t trajectoryCount() const = 0;
    virtual std::vector<std::string> trajectoryIds() const = 0;

    virtual std::vector<beamlab::data::TrajectorySample> getSamplesByTrajectory(
        const std::string& trajectoryId) const = 0;

    virtual std::vector<beamlab::data::TrajectorySample> getBatch(
        uint64_t offset, uint64_t count) const = 0;

    virtual std::vector<beamlab::data::TrajectorySample> getAxialRange(
        double zMin, double zMax) const = 0;

    // Called after bulk import to finalize storage (create indices, optimize).
    // No-op for in-memory backends.
    virtual void finalizeStorage() {}

    static std::unique_ptr<ISampleStorage> create(uint64_t estimatedFileSize);
};

} // namespace beamlab::core
