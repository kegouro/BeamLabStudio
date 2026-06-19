#pragma once

#include "services/storage/IStorageBackend.h"
#include "data/ids/TrajectoryId.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

struct sqlite3;
struct sqlite3_stmt;

namespace beamlab::services::storage {

class SqliteBackend final : public IStorageBackend {
public:
    explicit SqliteBackend(const std::string& dbPath);
    ~SqliteBackend() override;

    SqliteBackend(const SqliteBackend&) = delete;
    SqliteBackend& operator=(const SqliteBackend&) = delete;

    // ── Metadata ──────────────────────────────────────────────────

    std::string backendName() const override;
    uint64_t totalSamples() const override;
    uint64_t totalTrajectories() const override;

    // ── Write ─────────────────────────────────────────────────────

    void beginWriteBatch() override;
    void writeSample(const beamlab::data::TrajectorySample& sample) override;
    void endWriteBatch() override;

    // ── Read ──────────────────────────────────────────────────────

    SampleBatch readBatch(uint64_t offset, uint64_t count) const override;
    SampleBatch readAxialRange(double zMin, double zMax) const override;
    SampleBatch readTrajectory(beamlab::data::TrajectoryId id) const override;

    // ── Aggregate ─────────────────────────────────────────────────

    GlobalStats globalStats() const override;

    // ── Maintenance ───────────────────────────────────────────────

    void vacuum() override;
    uint64_t diskUsage() const override;

    // ── Bulk import helpers ───────────────────────────────────────

    void finalizeIndices();
    void setBatchSize(uint64_t batchSize) { batchSize_ = batchSize; }

private:
    void open(const std::string& dbPath);
    void close();

    void exec(const std::string& sql);
    void applyPerformancePragmas();
    void ensureTable();

    void flushPending();

    using StmtPtr = sqlite3_stmt*;
    StmtPtr prepare(const std::string& sql);
    void recycle(StmtPtr stmt);

    beamlab::data::TrajectorySample rowToSample(StmtPtr stmt) const;

    // Mutables for SampleBatch read methods (logically const).
    mutable std::vector<beamlab::data::TrajectorySample> readBuffer_;

    sqlite3* db_{nullptr};
    std::string dbPath_;
    std::vector<beamlab::data::TrajectorySample> pendingBatch_;
    uint64_t pendingCount_{0};
    uint64_t batchSize_{100000};
    bool indicesFinalized_{false};

    std::unordered_map<std::string, StmtPtr> stmtCache_;
};

} // namespace beamlab::services::storage
