#include "ccmcp/storage/sqlite/sqlite_index_run_store.h"

#include "ccmcp/indexing/index_run.h"

#include <sqlite3.h>
#include <stdexcept>
#include <string>

namespace ccmcp::storage::sqlite {

SqliteIndexRunStore::SqliteIndexRunStore(std::shared_ptr<SqliteDb> db) : db_(std::move(db)) {}

void SqliteIndexRunStore::upsert_run(const indexing::IndexRun& run) {
  const char* sql = R"(
    INSERT INTO index_runs
      (run_id, started_at, completed_at, provider_id, model_id, prompt_version, status, summary_json)
    VALUES (?, ?, ?, ?, ?, ?, ?, ?)
    ON CONFLICT(run_id) DO UPDATE SET
      started_at     = excluded.started_at,
      completed_at   = excluded.completed_at,
      provider_id    = excluded.provider_id,
      model_id       = excluded.model_id,
      prompt_version = excluded.prompt_version,
      status         = excluded.status,
      summary_json   = excluded.summary_json
  )";

  PreparedStatement stmt(db_->connection(), sql);
  if (!stmt.is_valid()) {
    return;
  }

  sqlite3_bind_text(stmt.get(), 1, run.run_id.c_str(), -1, SQLITE_TRANSIENT);

  if (run.started_at.has_value()) {
    sqlite3_bind_text(stmt.get(), 2, run.started_at.value().c_str(), -1, SQLITE_TRANSIENT);
  } else {
    sqlite3_bind_null(stmt.get(), 2);
  }

  if (run.completed_at.has_value()) {
    sqlite3_bind_text(stmt.get(), 3, run.completed_at.value().c_str(), -1, SQLITE_TRANSIENT);
  } else {
    sqlite3_bind_null(stmt.get(), 3);
  }

  sqlite3_bind_text(stmt.get(), 4, run.provider_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt.get(), 5, run.model_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt.get(), 6, run.prompt_version.c_str(), -1, SQLITE_TRANSIENT);

  const std::string status_str = indexing::index_run_status_to_string(run.status);
  sqlite3_bind_text(stmt.get(), 7, status_str.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt.get(), 8, run.summary_json.c_str(), -1, SQLITE_TRANSIENT);

  sqlite3_step(stmt.get());
}

void SqliteIndexRunStore::upsert_entry(const indexing::IndexEntry& entry) {
  const char* sql = R"(
    INSERT INTO index_entries
      (run_id, artifact_type, artifact_id, source_hash, vector_hash, indexed_at)
    VALUES (?, ?, ?, ?, ?, ?)
    ON CONFLICT(run_id, artifact_type, artifact_id) DO UPDATE SET
      source_hash  = excluded.source_hash,
      vector_hash  = excluded.vector_hash,
      indexed_at   = excluded.indexed_at
  )";

  PreparedStatement stmt(db_->connection(), sql);
  if (!stmt.is_valid()) {
    return;
  }

  sqlite3_bind_text(stmt.get(), 1, entry.run_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt.get(), 2, entry.artifact_type.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt.get(), 3, entry.artifact_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt.get(), 4, entry.source_hash.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt.get(), 5, entry.vector_hash.c_str(), -1, SQLITE_TRANSIENT);

  if (entry.indexed_at.has_value()) {
    sqlite3_bind_text(stmt.get(), 6, entry.indexed_at.value().c_str(), -1, SQLITE_TRANSIENT);
  } else {
    sqlite3_bind_null(stmt.get(), 6);
  }

  sqlite3_step(stmt.get());
}

std::optional<indexing::IndexRun> SqliteIndexRunStore::get_run(const std::string& run_id) const {
  const char* sql = "SELECT * FROM index_runs WHERE run_id = ?";

  PreparedStatement stmt(db_->connection(), sql);
  if (!stmt.is_valid()) {
    return std::nullopt;
  }

  sqlite3_bind_text(stmt.get(), 1, run_id.c_str(), -1, SQLITE_TRANSIENT);

  if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
    return row_to_run(stmt.get());
  }

  return std::nullopt;
}

std::vector<indexing::IndexRun> SqliteIndexRunStore::list_runs() const {
  const char* sql = "SELECT * FROM index_runs ORDER BY run_id";

  PreparedStatement stmt(db_->connection(), sql);
  if (!stmt.is_valid()) {
    return {};
  }

  std::vector<indexing::IndexRun> result;
  while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
    result.push_back(row_to_run(stmt.get()));
  }

  return result;
}

std::vector<indexing::IndexEntry> SqliteIndexRunStore::get_entries_for_run(
    const std::string& run_id) const {
  const char* sql =
      "SELECT * FROM index_entries WHERE run_id = ? "
      "ORDER BY artifact_type, artifact_id";

  PreparedStatement stmt(db_->connection(), sql);
  if (!stmt.is_valid()) {
    return {};
  }

  sqlite3_bind_text(stmt.get(), 1, run_id.c_str(), -1, SQLITE_TRANSIENT);

  std::vector<indexing::IndexEntry> result;
  while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
    result.push_back(row_to_entry(stmt.get()));
  }

  return result;
}

std::optional<std::string> SqliteIndexRunStore::get_last_source_hash(
    const std::string& artifact_id, const std::string& artifact_type,
    const std::string& provider_id, const std::string& model_id,
    const std::string& prompt_version) const {
  const char* sql = R"(
    SELECT ie.source_hash
    FROM index_entries ie
    JOIN index_runs ir ON ie.run_id = ir.run_id
    WHERE ie.artifact_id    = ?
      AND ie.artifact_type  = ?
      AND ir.provider_id    = ?
      AND ir.model_id       = ?
      AND ir.prompt_version = ?
      AND ir.status         = 'completed'
    ORDER BY ir.completed_at DESC
    LIMIT 1
  )";

  PreparedStatement stmt(db_->connection(), sql);
  if (!stmt.is_valid()) {
    return std::nullopt;
  }

  sqlite3_bind_text(stmt.get(), 1, artifact_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt.get(), 2, artifact_type.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt.get(), 3, provider_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt.get(), 4, model_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt.get(), 5, prompt_version.c_str(), -1, SQLITE_TRANSIENT);

  if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
    const auto* raw = sqlite3_column_text(stmt.get(), 0);
    if (raw != nullptr) {
      return std::string(reinterpret_cast<const char*>(raw));
    }
  }

  return std::nullopt;
}

// Column order for SELECT * FROM index_runs:
//   0: run_id, 1: started_at, 2: completed_at, 3: provider_id,
//   4: model_id, 5: prompt_version, 6: status, 7: summary_json
indexing::IndexRun SqliteIndexRunStore::row_to_run(sqlite3_stmt* stmt) const {
  indexing::IndexRun run;

  run.run_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));

  if (sqlite3_column_type(stmt, 1) != SQLITE_NULL) {
    run.started_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
  }

  if (sqlite3_column_type(stmt, 2) != SQLITE_NULL) {
    run.completed_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
  }

  run.provider_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
  run.model_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
  run.prompt_version = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));

  const std::string status_str = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
  run.status = indexing::index_run_status_from_string(status_str);

  run.summary_json = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));

  return run;
}

std::string SqliteIndexRunStore::next_index_run_id() {
  // Atomically increment the 'index_run' counter and return "run-N".
  //
  // BEGIN IMMEDIATE acquires a write lock before any reads, which prevents two
  // processes from reading the same counter value simultaneously on a shared
  // file-based database.
  char* err_msg = nullptr;

  if (sqlite3_exec(db_->connection(), "BEGIN IMMEDIATE", nullptr, nullptr, &err_msg) != SQLITE_OK) {
    const std::string msg = err_msg != nullptr ? err_msg : "unknown error";
    sqlite3_free(err_msg);
    throw std::runtime_error("next_index_run_id: failed to begin transaction: " + msg);
  }

  // Insert with value=1 if the row is absent; otherwise increment the existing value.
  const char* upsert_sql = R"(
    INSERT INTO id_counters (name, value) VALUES ('index_run', 1)
    ON CONFLICT(name) DO UPDATE SET value = value + 1
  )";
  if (sqlite3_exec(db_->connection(), upsert_sql, nullptr, nullptr, &err_msg) != SQLITE_OK) {
    const std::string msg = err_msg != nullptr ? err_msg : "unknown error";
    sqlite3_free(err_msg);
    sqlite3_exec(db_->connection(), "ROLLBACK", nullptr, nullptr, nullptr);
    throw std::runtime_error("next_index_run_id: failed to increment counter: " + msg);
  }

  // Read the updated value.
  const char* select_sql = "SELECT value FROM id_counters WHERE name = 'index_run'";
  PreparedStatement stmt(db_->connection(), select_sql);
  if (!stmt.is_valid()) {
    sqlite3_exec(db_->connection(), "ROLLBACK", nullptr, nullptr, nullptr);
    throw std::runtime_error("next_index_run_id: failed to prepare select: " + stmt.error());
  }

  if (sqlite3_step(stmt.get()) != SQLITE_ROW) {
    sqlite3_exec(db_->connection(), "ROLLBACK", nullptr, nullptr, nullptr);
    throw std::runtime_error("next_index_run_id: counter row missing after upsert");
  }

  const long long value = sqlite3_column_int64(stmt.get(), 0);

  if (sqlite3_exec(db_->connection(), "COMMIT", nullptr, nullptr, &err_msg) != SQLITE_OK) {
    const std::string msg = err_msg != nullptr ? err_msg : "unknown error";
    sqlite3_free(err_msg);
    throw std::runtime_error("next_index_run_id: failed to commit: " + msg);
  }

  return "run-" + std::to_string(value);
}

// Column order for SELECT * FROM index_entries:
//   0: run_id, 1: artifact_type, 2: artifact_id,
//   3: source_hash, 4: vector_hash, 5: indexed_at
indexing::IndexEntry SqliteIndexRunStore::row_to_entry(sqlite3_stmt* stmt) const {
  indexing::IndexEntry entry;

  entry.run_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
  entry.artifact_type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
  entry.artifact_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
  entry.source_hash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
  entry.vector_hash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));

  if (sqlite3_column_type(stmt, 5) != SQLITE_NULL) {
    entry.indexed_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
  }

  return entry;
}

}  // namespace ccmcp::storage::sqlite
