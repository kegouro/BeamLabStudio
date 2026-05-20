#pragma once

#include "core/storage/ISampleStorage.h"

#include <string>
#include <vector>

struct sqlite3;
struct sqlite3_stmt;

namespace beamlab::data {
struct TrajectorySample;
} // namespace beamlab::data

namespace beamlab::core {

class SqliteStorage final : public ISampleStorage {
public:
    explicit SqliteStorage(const std::string& dbPath = ":memory:");
    ~SqliteStorage() override;

    SqliteStorage(const SqliteStorage&) = delete;
    SqliteStorage& operator=(const SqliteStorage&) = delete;
    SqliteStorage(SqliteStorage&&) = delete;
    SqliteStorage& operator=(SqliteStorage&&) = delete;

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

private:
    void ensureTable();
    void exec(const char* sql);
    beamlab::data::TrajectorySample rowToSample(sqlite3_stmt* stmt) const;

    sqlite3* db_{nullptr};
    std::string dbPath_;
    std::string currentTrajectoryId_;
    uint64_t pendingCount_{0};
    static constexpr uint64_t kFlushBatchSize = 50000;
};

} // namespace beamlab::core
