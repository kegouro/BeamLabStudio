#pragma once

#include "data/model/TrajectorySample.h"
#include "data/ids/TrajectoryId.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace beamlab::services::storage {

struct SampleBatch {
    const beamlab::data::TrajectorySample* data{nullptr};
    uint64_t count{0};
    uint64_t offset{0};
};

struct GlobalStats {
    double zMin{0.0};
    double zMax{0.0};
    double edepSum{0.0};
    uint64_t count{0};
};

class IStorageBackend {
public:
    virtual ~IStorageBackend() = default;

    // ── Metadata ───────────────────────────────────────────────────────

    virtual std::string backendName() const = 0;
    virtual uint64_t totalSamples() const = 0;
    virtual uint64_t totalTrajectories() const = 0;

    // ── Write ──────────────────────────────────────────────────────────

    virtual void beginWriteBatch() = 0;
    virtual void writeSample(const beamlab::data::TrajectorySample& sample) = 0;
    virtual void endWriteBatch() = 0;

    // ── Read ───────────────────────────────────────────────────────────

    virtual SampleBatch readBatch(uint64_t offset, uint64_t count) const = 0;
    virtual SampleBatch readAxialRange(double zMin, double zMax) const = 0;
    virtual SampleBatch readTrajectory(beamlab::data::TrajectoryId id) const = 0;

    // ── Aggregate queries ──────────────────────────────────────────────

    virtual GlobalStats globalStats() const = 0;

    // ── Maintenance ────────────────────────────────────────────────────

    virtual void vacuum() = 0;
    virtual uint64_t diskUsage() const = 0;
};

} // namespace beamlab::services::storage
