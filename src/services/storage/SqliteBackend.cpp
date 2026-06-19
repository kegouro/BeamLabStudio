#include "services/storage/SqliteBackend.h"

#include <sqlite3.h>

#include <cstring>
#include <iostream>
#include <stdexcept>

namespace beamlab::services::storage {

// ── Construction / Destruction ─────────────────────────────────────

SqliteBackend::SqliteBackend(const std::string& dbPath)
    : dbPath_(dbPath)
{
    open(dbPath_);
    applyPerformancePragmas();
    ensureTable();
}

SqliteBackend::~SqliteBackend()
{
    try {
        flushPending();
    } catch (...) {
    }
    close();
}

// ── SQLite helpers ─────────────────────────────────────────────────

void SqliteBackend::open(const std::string& dbPath)
{
    if (sqlite3_open(dbPath.c_str(), &db_) != SQLITE_OK) {
        auto msg = std::string("Failed to open database: ") +
                   (db_ ? sqlite3_errmsg(db_) : "unknown");
        db_ = nullptr;
        throw std::runtime_error(msg);
    }
}

void SqliteBackend::close()
{
    for (auto& [_, stmt] : stmtCache_) {
        sqlite3_finalize(stmt);
    }
    stmtCache_.clear();

    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

void SqliteBackend::exec(const std::string& sql)
{
    char* err = nullptr;
    if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err) != SQLITE_OK) {
        std::string msg = err ? err : "unknown error";
        sqlite3_free(err);
        throw std::runtime_error("SQLite exec error: " + msg);
    }
}

void SqliteBackend::applyPerformancePragmas()
{
    exec("PRAGMA journal_mode=WAL");
    exec("PRAGMA synchronous=NORMAL");
    exec("PRAGMA cache_size=-65536");          // 64 MB page cache
    exec("PRAGMA temp_store=MEMORY");
    exec("PRAGMA mmap_size=268435456");        // 256 MB memory-mapped I/O
    exec("PRAGMA page_size=4096");
    exec("PRAGMA locking_mode=EXCLUSIVE");
    exec("PRAGMA foreign_keys=OFF");
    exec("PRAGMA automatic_index=OFF");
}

void SqliteBackend::ensureTable()
{
    exec(
        "CREATE TABLE IF NOT EXISTS samples ("
        "  sample_id     INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  trajectory_id INTEGER NOT NULL,"
        "  step_index    INTEGER NOT NULL,"
        "  x_m           REAL NOT NULL,"
        "  y_m           REAL NOT NULL,"
        "  z_m           REAL NOT NULL,"
        "  edep_MeV      REAL NOT NULL DEFAULT 0.0,"
        "  kinE_MeV      REAL NOT NULL DEFAULT 0.0,"
        "  momx_MeV      REAL NOT NULL DEFAULT 0.0,"
        "  momy_MeV      REAL NOT NULL DEFAULT 0.0,"
        "  momz_MeV      REAL NOT NULL DEFAULT 0.0,"
        "  time_s        REAL NOT NULL DEFAULT 0.0,"
        "  dose_Gy       REAL NOT NULL DEFAULT 0.0"
        ")");
}

// ── Prepared statement cache ──────────────────────────────────────

SqliteBackend::StmtPtr SqliteBackend::prepare(const std::string& sql)
{
    auto it = stmtCache_.find(sql);
    if (it != stmtCache_.end()) {
        sqlite3_reset(it->second);
        sqlite3_clear_bindings(it->second);
        return it->second;
    }

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error(std::string("SQLite prepare error: ") +
                                  sqlite3_errmsg(db_));
    }
    stmtCache_[sql] = stmt;
    return stmt;
}

void SqliteBackend::recycle(StmtPtr stmt)
{
    sqlite3_reset(stmt);
    sqlite3_clear_bindings(stmt);
}

// ── Row conversion ────────────────────────────────────────────────

beamlab::data::TrajectorySample SqliteBackend::rowToSample(StmtPtr stmt) const
{
    beamlab::data::TrajectorySample s;
    s.trajectory_id = beamlab::data::TrajectoryId(
        static_cast<uint64_t>(sqlite3_column_int64(stmt, 0)));
    s.position_m.x = sqlite3_column_double(stmt, 2);
    s.position_m.y = sqlite3_column_double(stmt, 3);
    s.position_m.z = sqlite3_column_double(stmt, 4);
    s.edep_MeV = sqlite3_column_double(stmt, 5);
    s.kinE_MeV = sqlite3_column_double(stmt, 6);
    s.momentum_MeV.x = sqlite3_column_double(stmt, 7);
    s.momentum_MeV.y = sqlite3_column_double(stmt, 8);
    s.momentum_MeV.z = sqlite3_column_double(stmt, 9);
    s.time_s = sqlite3_column_double(stmt, 10);
    s.dose_Gy = sqlite3_column_double(stmt, 11);
    return s;
}

// ── Metadata ──────────────────────────────────────────────────────

std::string SqliteBackend::backendName() const
{
    return "SQLite3 (" + dbPath_ + ")";
}

uint64_t SqliteBackend::totalSamples() const
{
    auto* stmt = prepare("SELECT COUNT(*) FROM samples");
    if (sqlite3_step(stmt) != SQLITE_ROW) return 0;
    auto count = static_cast<uint64_t>(sqlite3_column_int64(stmt, 0));
    recycle(stmt);
    return count;
}

uint64_t SqliteBackend::totalTrajectories() const
{
    auto* stmt = prepare("SELECT COUNT(DISTINCT trajectory_id) FROM samples");
    if (sqlite3_step(stmt) != SQLITE_ROW) return 0;
    auto count = static_cast<uint64_t>(sqlite3_column_int64(stmt, 0));
    recycle(stmt);
    return count;
}

// ── Write ─────────────────────────────────────────────────────────

void SqliteBackend::beginWriteBatch()
{
    exec("BEGIN TRANSACTION");
}

void SqliteBackend::writeSample(const beamlab::data::TrajectorySample& sample)
{
    pendingBatch_.push_back(sample);
    ++pendingCount_;
    if (pendingBatch_.size() >= batchSize_) {
        flushPending();
    }
}

void SqliteBackend::endWriteBatch()
{
    if (!pendingBatch_.empty()) {
        flushPending();
    }
    exec("COMMIT");
}

void SqliteBackend::flushPending()
{
    if (pendingBatch_.empty()) return;

    auto* stmt = prepare(
        "INSERT INTO samples(trajectory_id,step_index,x_m,y_m,z_m,"
        "edep_MeV,kinE_MeV,momx_MeV,momy_MeV,momz_MeV,time_s,dose_Gy) "
        "VALUES(?,?,?,?,?,?,?,?,?,?,?,?)");

    for (const auto& s : pendingBatch_) {
        sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(s.trajectory_id.value()));
        sqlite3_bind_int64(stmt, 2, 0);
        sqlite3_bind_double(stmt, 3, s.position_m.x);
        sqlite3_bind_double(stmt, 4, s.position_m.y);
        sqlite3_bind_double(stmt, 5, s.position_m.z);
        sqlite3_bind_double(stmt, 6, s.edep_MeV);
        sqlite3_bind_double(stmt, 7, s.kinE_MeV);
        sqlite3_bind_double(stmt, 8, s.momentum_MeV.x);
        sqlite3_bind_double(stmt, 9, s.momentum_MeV.y);
        sqlite3_bind_double(stmt, 10, s.momentum_MeV.z);
        sqlite3_bind_double(stmt, 11, s.time_s);
        sqlite3_bind_double(stmt, 12, s.dose_Gy);
        sqlite3_step(stmt);
        recycle(stmt);
    }
    pendingBatch_.clear();
}

// ── Read ──────────────────────────────────────────────────────────

SampleBatch SqliteBackend::readBatch(uint64_t offset, uint64_t count) const
{
    readBuffer_.clear();
    readBuffer_.reserve(count);

    auto* stmt = prepare(
        "SELECT trajectory_id,step_index,x_m,y_m,z_m,"
        "edep_MeV,kinE_MeV,momx_MeV,momy_MeV,momz_MeV,time_s,dose_Gy "
        "FROM samples ORDER BY sample_id LIMIT ? OFFSET ?");
    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(count));
    sqlite3_bind_int64(stmt, 2, static_cast<int64_t>(offset));

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        readBuffer_.push_back(rowToSample(stmt));
    }
    recycle(stmt);

    return SampleBatch{readBuffer_.data(), readBuffer_.size(), offset};
}

SampleBatch SqliteBackend::readAxialRange(double zMin, double zMax) const
{
    readBuffer_.clear();

    auto* stmt = prepare(
        "SELECT trajectory_id,step_index,x_m,y_m,z_m,"
        "edep_MeV,kinE_MeV,momx_MeV,momy_MeV,momz_MeV,time_s,dose_Gy "
        "FROM samples WHERE z_m BETWEEN ? AND ? ORDER BY z_m");
    sqlite3_bind_double(stmt, 1, zMin);
    sqlite3_bind_double(stmt, 2, zMax);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        readBuffer_.push_back(rowToSample(stmt));
    }
    recycle(stmt);

    return SampleBatch{readBuffer_.data(), readBuffer_.size(), 0};
}

SampleBatch SqliteBackend::readTrajectory(beamlab::data::TrajectoryId id) const
{
    readBuffer_.clear();

    auto* stmt = prepare(
        "SELECT trajectory_id,step_index,x_m,y_m,z_m,"
        "edep_MeV,kinE_MeV,momx_MeV,momy_MeV,momz_MeV,time_s,dose_Gy "
        "FROM samples WHERE trajectory_id = ? ORDER BY step_index");
    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(id.value()));

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        readBuffer_.push_back(rowToSample(stmt));
    }
    recycle(stmt);

    return SampleBatch{readBuffer_.data(), readBuffer_.size(), 0};
}

// ── Aggregate ─────────────────────────────────────────────────────

GlobalStats SqliteBackend::globalStats() const
{
    auto* stmt = prepare(
        "SELECT MIN(z_m), MAX(z_m), COALESCE(SUM(edep_MeV),0), COUNT(*) FROM samples");
    GlobalStats stats{};
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        stats.zMin = sqlite3_column_double(stmt, 0);
        stats.zMax = sqlite3_column_double(stmt, 1);
        stats.edepSum = sqlite3_column_double(stmt, 2);
        stats.count = static_cast<uint64_t>(sqlite3_column_int64(stmt, 3));
    }
    recycle(stmt);
    return stats;
}

// ── Maintenance ───────────────────────────────────────────────────

void SqliteBackend::vacuum()
{
    exec("VACUUM");
}

uint64_t SqliteBackend::diskUsage() const
{
    auto* stmt = prepare(
        "SELECT COALESCE(SUM(pgsize),0) FROM dbstat");
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        auto bytes = static_cast<uint64_t>(sqlite3_column_int64(stmt, 0));
        recycle(stmt);
        return bytes;
    }
    recycle(stmt);
    return 0;
}

// ── Bulk import helpers ───────────────────────────────────────────

void SqliteBackend::finalizeIndices()
{
    if (indicesFinalized_) return;

    exec("CREATE INDEX IF NOT EXISTS idx_traj_step "
         "ON samples(trajectory_id, step_index)");
    exec("CREATE INDEX IF NOT EXISTS idx_axial ON samples(z_m)");
    exec("ANALYZE samples");

    indicesFinalized_ = true;
}

} // namespace beamlab::services::storage
