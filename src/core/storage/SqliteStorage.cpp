#include "core/storage/SqliteStorage.h"

#include "data/model/TrajectorySample.h"

#include <sqlite3.h>

#include <cstdio>
#include <stdexcept>
#include <string>

namespace beamlab::core {

SqliteStorage::SqliteStorage(const std::string& dbPath)
    : dbPath_(dbPath)
{
    if (sqlite3_open(dbPath_.c_str(), &db_) != SQLITE_OK) {
        throw std::runtime_error("Failed to open SQLite database: " + dbPath_);
    }
    exec("PRAGMA journal_mode=MEMORY");     // Fast, journal in RAM (safe for temp files)
    exec("PRAGMA synchronous=OFF");         // Fast bulk import — restored in finalizeIndices()
    exec("PRAGMA cache_size=-8000");        // 8 MB page cache (minimal, for < 2 GB RAM)
    exec("PRAGMA locking_mode=EXCLUSIVE");   // Single writer, no lock overhead
    ensureTable();
}

SqliteStorage::~SqliteStorage()
{
    flush();
    if (insertStmt_) sqlite3_finalize(insertStmt_);
    if (db_) sqlite3_close(db_);
}

void SqliteStorage::ensureTable()
{
    exec("CREATE TABLE IF NOT EXISTS samples ("
         " sample_id INTEGER PRIMARY KEY AUTOINCREMENT,"
         " trajectory_id TEXT NOT NULL,"
         " step_index INTEGER NOT NULL,"
         " x_m REAL NOT NULL,"
         " y_m REAL NOT NULL,"
         " z_m REAL NOT NULL,"
         " edep_MeV REAL NOT NULL DEFAULT 0.0,"
         " kinE_MeV REAL NOT NULL DEFAULT 0.0,"
         " momx_MeV REAL NOT NULL DEFAULT 0.0,"
         " momy_MeV REAL NOT NULL DEFAULT 0.0,"
         " momz_MeV REAL NOT NULL DEFAULT 0.0,"
         " time_s REAL NOT NULL DEFAULT 0.0,"
         " dose_Gy REAL NOT NULL DEFAULT 0.0)");
    // Indices created AFTER bulk import via finalizeIndices()

    const char* sql = "INSERT INTO samples(trajectory_id,step_index,x_m,y_m,z_m,"
                      "edep_MeV,kinE_MeV,momx_MeV,momy_MeV,momz_MeV,time_s,dose_Gy) "
                      "VALUES(?,?,?,?,?,?,?,?,?,?,?,?)";
    sqlite3_prepare_v2(db_, sql, -1, &insertStmt_, nullptr);
}

void SqliteStorage::finalizeIndices()
{
    exec("PRAGMA journal_mode=WAL");
    exec("PRAGMA synchronous=NORMAL");
    exec("PRAGMA locking_mode=NORMAL");
    exec("CREATE INDEX IF NOT EXISTS idx_traj ON samples(trajectory_id, step_index)");
    exec("CREATE INDEX IF NOT EXISTS idx_axial ON samples(z_m)");
}

std::pair<double, double> SqliteStorage::getZRange() const
{
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, "SELECT MIN(z_m), MAX(z_m) FROM samples", -1, &stmt, nullptr);
    double zMin = 0.0, zMax = 0.0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        zMin = sqlite3_column_double(stmt, 0);
        zMax = sqlite3_column_double(stmt, 1);
    }
    sqlite3_finalize(stmt);
    return {zMin, zMax};
}

void SqliteStorage::exec(const char* sql)
{
    char* err = nullptr;
    if (sqlite3_exec(db_, sql, nullptr, nullptr, &err) != SQLITE_OK) {
        std::string msg = err ? err : "unknown";
        sqlite3_free(err);
        throw std::runtime_error("SQLite error: " + msg);
    }
}

void SqliteStorage::beginTrajectory(const std::string& trajectoryId)
{
    flush();
    currentTrajectoryId_ = trajectoryId;
}

void SqliteStorage::addSample(const beamlab::data::TrajectorySample& sample)
{
    const char* trajId = currentTrajectoryId_.c_str();
    const int64_t stepIdx = static_cast<int64_t>(pendingCount_);

    sqlite3_bind_text(insertStmt_, 1, trajId, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(insertStmt_, 2, stepIdx);
    sqlite3_bind_double(insertStmt_, 3, sample.position_m.x);
    sqlite3_bind_double(insertStmt_, 4, sample.position_m.y);
    sqlite3_bind_double(insertStmt_, 5, sample.position_m.z);
    sqlite3_bind_double(insertStmt_, 6, sample.edep_MeV);
    sqlite3_bind_double(insertStmt_, 7, sample.kinE_MeV);
    sqlite3_bind_double(insertStmt_, 8, sample.momentum_MeV.x);
    sqlite3_bind_double(insertStmt_, 9, sample.momentum_MeV.y);
    sqlite3_bind_double(insertStmt_, 10, sample.momentum_MeV.z);
    sqlite3_bind_double(insertStmt_, 11, sample.time_s);
    sqlite3_bind_double(insertStmt_, 12, sample.dose_Gy);

    sqlite3_step(insertStmt_);
    sqlite3_reset(insertStmt_);
    ++pendingCount_;
}

void SqliteStorage::endTrajectory()
{
    flush();
    currentTrajectoryId_.clear();
    pendingCount_ = 0;
}

void SqliteStorage::flush()
{
    // Data is inserted immediately via addSample.
    // This method exists for interface compliance.
}

uint64_t SqliteStorage::totalSampleCount() const
{
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, "SELECT COUNT(*) FROM samples", -1, &stmt, nullptr);
    uint64_t count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = static_cast<uint64_t>(sqlite3_column_int64(stmt, 0));
    }
    sqlite3_finalize(stmt);
    return count;
}

uint64_t SqliteStorage::trajectoryCount() const
{
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, "SELECT COUNT(DISTINCT trajectory_id) FROM samples", -1, &stmt, nullptr);
    uint64_t count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = static_cast<uint64_t>(sqlite3_column_int64(stmt, 0));
    }
    sqlite3_finalize(stmt);
    return count;
}

std::vector<std::string> SqliteStorage::trajectoryIds() const
{
    std::vector<std::string> ids;
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, "SELECT DISTINCT trajectory_id FROM samples ORDER BY trajectory_id",
                       -1, &stmt, nullptr);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ids.push_back(std::string(
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0))));
    }
    sqlite3_finalize(stmt);
    return ids;
}

beamlab::data::TrajectorySample SqliteStorage::rowToSample(sqlite3_stmt* stmt) const
{
    beamlab::data::TrajectorySample s{};
    s.position_m.x = sqlite3_column_double(stmt, 0);
    s.position_m.y = sqlite3_column_double(stmt, 1);
    s.position_m.z = sqlite3_column_double(stmt, 2);
    s.edep_MeV     = sqlite3_column_double(stmt, 3);
    s.kinE_MeV     = sqlite3_column_double(stmt, 4);
    s.momentum_MeV.x = sqlite3_column_double(stmt, 5);
    s.momentum_MeV.y = sqlite3_column_double(stmt, 6);
    s.momentum_MeV.z = sqlite3_column_double(stmt, 7);
    s.time_s       = sqlite3_column_double(stmt, 8);
    s.dose_Gy      = sqlite3_column_double(stmt, 9);
    s.edep_eV      = s.edep_MeV * 1.0e6;
    return s;
}

std::vector<beamlab::data::TrajectorySample> SqliteStorage::getSamplesByTrajectory(
    const std::string& trajectoryId) const
{
    std::vector<beamlab::data::TrajectorySample> result;
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_,
        "SELECT x_m,y_m,z_m,edep_MeV,kinE_MeV,momx_MeV,momy_MeV,momz_MeV,time_s,dose_Gy "
        "FROM samples WHERE trajectory_id=? ORDER BY step_index",
        -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, trajectoryId.c_str(), -1, SQLITE_TRANSIENT);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        result.push_back(rowToSample(stmt));
    }
    sqlite3_finalize(stmt);
    return result;
}

std::vector<beamlab::data::TrajectorySample> SqliteStorage::getBatch(
    uint64_t offset, uint64_t count) const
{
    std::vector<beamlab::data::TrajectorySample> result;
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_,
        "SELECT x_m,y_m,z_m,edep_MeV,kinE_MeV,momx_MeV,momy_MeV,momz_MeV,time_s,dose_Gy "
        "FROM samples ORDER BY sample_id LIMIT ? OFFSET ?",
        -1, &stmt, nullptr);
    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(count));
    sqlite3_bind_int64(stmt, 2, static_cast<int64_t>(offset));
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        result.push_back(rowToSample(stmt));
    }
    sqlite3_finalize(stmt);
    return result;
}

std::vector<beamlab::data::TrajectorySample> SqliteStorage::getAxialRange(
    double zMin, double zMax) const
{
    std::vector<beamlab::data::TrajectorySample> result;
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_,
        "SELECT x_m,y_m,z_m,edep_MeV,kinE_MeV,momx_MeV,momy_MeV,momz_MeV,time_s,dose_Gy "
        "FROM samples WHERE z_m >= ? AND z_m < ? ORDER BY z_m",
        -1, &stmt, nullptr);
    sqlite3_bind_double(stmt, 1, zMin);
    sqlite3_bind_double(stmt, 2, zMax);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        result.push_back(rowToSample(stmt));
    }
    sqlite3_finalize(stmt);
    return result;
}

} // namespace beamlab::core
