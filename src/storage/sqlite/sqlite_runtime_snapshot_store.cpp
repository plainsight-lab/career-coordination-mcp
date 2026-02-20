#include "ccmcp/storage/sqlite/sqlite_runtime_snapshot_store.h"

#include <sqlite3.h>
#include <stdexcept>
#include <string>

namespace ccmcp::storage::sqlite {

SqliteRuntimeSnapshotStore::SqliteRuntimeSnapshotStore(std::shared_ptr<SqliteDb> db)
    : db_(std::move(db)) {}

void SqliteRuntimeSnapshotStore::save(const std::string& run_id, const std::string& snapshot_json,
                                      const std::string& snapshot_hash,
                                      const std::string& created_at) {
  const char* sql = R"(
    INSERT INTO runtime_snapshots (run_id, snapshot_json, snapshot_hash, created_at)
    VALUES (?, ?, ?, ?)
  )";

  PreparedStatement stmt(db_->connection(), sql);
  if (!stmt.is_valid()) {
    throw std::runtime_error("SqliteRuntimeSnapshotStore::save failed to prepare: " + stmt.error());
  }

  sqlite3_bind_text(stmt.get(), 1, run_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt.get(), 2, snapshot_json.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt.get(), 3, snapshot_hash.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt.get(), 4, created_at.c_str(), -1, SQLITE_TRANSIENT);

  const int rc = sqlite3_step(stmt.get());
  if (rc != SQLITE_DONE) {
    throw std::runtime_error("SqliteRuntimeSnapshotStore::save failed: " +
                             std::string(sqlite3_errmsg(db_->connection())));
  }
}

std::optional<std::string> SqliteRuntimeSnapshotStore::get_snapshot_json(
    const std::string& run_id) const {
  const char* sql = "SELECT snapshot_json FROM runtime_snapshots WHERE run_id = ?";

  PreparedStatement stmt(db_->connection(), sql);
  if (!stmt.is_valid()) {
    return std::nullopt;
  }

  sqlite3_bind_text(stmt.get(), 1, run_id.c_str(), -1, SQLITE_TRANSIENT);

  if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
    return std::string{
        reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 0))};  // NOLINT
  }

  return std::nullopt;
}

}  // namespace ccmcp::storage::sqlite
