#include "ccmcp/storage/sqlite/sqlite_db.h"

#include <sqlite3.h>

namespace ccmcp::storage::sqlite {

// Deleter implementations
void SqliteDb::SqliteDeleter::operator()(sqlite3* db) const {
  if (db != nullptr) {
    sqlite3_close(db);
  }
}

void PreparedStatement::StmtDeleter::operator()(sqlite3_stmt* stmt) const {
  if (stmt != nullptr) {
    sqlite3_finalize(stmt);
  }
}

// Embedded schema v1 SQL
constexpr const char* kSchemaV1 = R"(
PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS schema_version (
  version INTEGER PRIMARY KEY,
  applied_at TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS atoms (
  atom_id TEXT PRIMARY KEY,
  domain TEXT NOT NULL,
  title TEXT NOT NULL,
  claim TEXT NOT NULL,
  tags_json TEXT NOT NULL,
  verified INTEGER NOT NULL CHECK(verified IN (0, 1)),
  evidence_refs_json TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_atoms_verified ON atoms(verified);

CREATE TABLE IF NOT EXISTS opportunities (
  opportunity_id TEXT PRIMARY KEY,
  company TEXT NOT NULL,
  role_title TEXT NOT NULL,
  source TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS requirements (
  opportunity_id TEXT NOT NULL,
  idx INTEGER NOT NULL,
  text TEXT NOT NULL,
  tags_json TEXT NOT NULL,
  required INTEGER NOT NULL CHECK(required IN (0, 1)),
  PRIMARY KEY(opportunity_id, idx),
  FOREIGN KEY(opportunity_id) REFERENCES opportunities(opportunity_id)
    ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS interactions (
  interaction_id TEXT PRIMARY KEY,
  contact_id TEXT NOT NULL,
  opportunity_id TEXT NOT NULL,
  state INTEGER NOT NULL,
  FOREIGN KEY(opportunity_id) REFERENCES opportunities(opportunity_id)
);

CREATE TABLE IF NOT EXISTS audit_events (
  event_id TEXT PRIMARY KEY,
  trace_id TEXT NOT NULL,
  event_type TEXT NOT NULL,
  payload TEXT NOT NULL,
  created_at TEXT NOT NULL,
  entity_ids_json TEXT NOT NULL,
  idx INTEGER NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_audit_events_trace ON audit_events(trace_id, idx);

INSERT OR IGNORE INTO schema_version (version, applied_at)
VALUES (1, datetime('now'));
)";

// Embedded schema v2 SQL (adds resume tables)
constexpr const char* kSchemaV2 = R"(
CREATE TABLE IF NOT EXISTS resumes (
  resume_id TEXT PRIMARY KEY,
  resume_md TEXT NOT NULL,
  resume_hash TEXT NOT NULL UNIQUE,
  created_at TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_resumes_hash ON resumes(resume_hash);

CREATE TABLE IF NOT EXISTS resume_meta (
  resume_id TEXT PRIMARY KEY,
  source_path TEXT,
  source_hash TEXT NOT NULL,
  extraction_method TEXT NOT NULL,
  extracted_at TEXT,
  ingestion_version TEXT NOT NULL,
  FOREIGN KEY(resume_id) REFERENCES resumes(resume_id)
    ON DELETE CASCADE
);

INSERT OR IGNORE INTO schema_version (version, applied_at)
VALUES (2, datetime('now'));
)";

// Embedded schema v3 SQL (adds token IR table)
constexpr const char* kSchemaV3 = R"(
CREATE TABLE IF NOT EXISTS resume_token_ir (
  token_ir_id TEXT PRIMARY KEY,
  resume_id TEXT NOT NULL,
  token_ir_json TEXT NOT NULL,
  created_at TEXT NOT NULL,
  FOREIGN KEY(resume_id) REFERENCES resumes(resume_id)
    ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_resume_token_ir_resume ON resume_token_ir(resume_id);

INSERT OR IGNORE INTO schema_version (version, applied_at)
VALUES (3, datetime('now'));
)";

// Embedded schema v4 SQL (adds index_runs + index_entries for embedding lifecycle)
constexpr const char* kSchemaV4 = R"(
CREATE TABLE IF NOT EXISTS index_runs (
  run_id TEXT PRIMARY KEY,
  started_at TEXT,
  completed_at TEXT,
  provider_id TEXT NOT NULL,
  model_id TEXT NOT NULL,
  prompt_version TEXT NOT NULL,
  status TEXT NOT NULL,
  summary_json TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS index_entries (
  run_id TEXT NOT NULL,
  artifact_type TEXT NOT NULL,
  artifact_id TEXT NOT NULL,
  source_hash TEXT NOT NULL,
  vector_hash TEXT NOT NULL,
  indexed_at TEXT,
  PRIMARY KEY (run_id, artifact_type, artifact_id),
  FOREIGN KEY(run_id) REFERENCES index_runs(run_id) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_index_entries_artifact
  ON index_entries(artifact_type, artifact_id);

INSERT OR IGNORE INTO schema_version (version, applied_at)
VALUES (4, datetime('now'));
)";

SqliteDb::SqliteDb(sqlite3* db) : db_(db) {}

core::Result<std::shared_ptr<SqliteDb>, std::string> SqliteDb::open(const std::string& path) {
  sqlite3* db = nullptr;
  int rc = sqlite3_open(path.c_str(), &db);
  if (rc != SQLITE_OK) {
    std::string error = sqlite3_errmsg(db);
    sqlite3_close(db);
    return core::Result<std::shared_ptr<SqliteDb>, std::string>::err("Failed to open database: " +
                                                                     error);
  }

  // Enable foreign keys
  char* err_msg = nullptr;
  rc = sqlite3_exec(db, "PRAGMA foreign_keys = ON;", nullptr, nullptr, &err_msg);
  if (rc != SQLITE_OK) {
    std::string error = err_msg != nullptr ? err_msg : "Unknown error";
    sqlite3_free(err_msg);
    sqlite3_close(db);
    return core::Result<std::shared_ptr<SqliteDb>, std::string>::err(
        "Failed to enable foreign keys: " + error);
  }

  return core::Result<std::shared_ptr<SqliteDb>, std::string>::ok(
      std::shared_ptr<SqliteDb>(new SqliteDb(db)));
}

int SqliteDb::get_schema_version() const {
  // Check if schema_version table exists
  sqlite3_stmt* stmt = nullptr;
  const char* sql = "SELECT version FROM schema_version ORDER BY version DESC LIMIT 1";
  int rc = sqlite3_prepare_v2(db_.get(), sql, -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    return 0;  // Table doesn't exist yet
  }

  int version = 0;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    version = sqlite3_column_int(stmt, 0);
  }

  sqlite3_finalize(stmt);
  return version;
}

core::Result<bool, std::string> SqliteDb::ensure_schema_v1() {
  if (get_schema_version() >= 1) {
    return core::Result<bool, std::string>::ok(true);
  }

  char* err_msg = nullptr;
  int rc = sqlite3_exec(db_.get(), kSchemaV1, nullptr, nullptr, &err_msg);
  if (rc != SQLITE_OK) {
    std::string error = err_msg != nullptr ? err_msg : "Unknown error";
    sqlite3_free(err_msg);
    return core::Result<bool, std::string>::err("Failed to apply schema v1: " + error);
  }

  return core::Result<bool, std::string>::ok(true);
}

core::Result<bool, std::string> SqliteDb::ensure_schema_v2() {
  // Ensure v1 is applied first
  auto v1_result = ensure_schema_v1();
  if (!v1_result.has_value()) {
    return v1_result;
  }

  if (get_schema_version() >= 2) {
    return core::Result<bool, std::string>::ok(true);
  }

  char* err_msg = nullptr;
  int rc = sqlite3_exec(db_.get(), kSchemaV2, nullptr, nullptr, &err_msg);
  if (rc != SQLITE_OK) {
    std::string error = err_msg != nullptr ? err_msg : "Unknown error";
    sqlite3_free(err_msg);
    return core::Result<bool, std::string>::err("Failed to apply schema v2: " + error);
  }

  return core::Result<bool, std::string>::ok(true);
}

core::Result<bool, std::string> SqliteDb::ensure_schema_v3() {
  // Ensure v2 is applied first
  auto v2_result = ensure_schema_v2();
  if (!v2_result.has_value()) {
    return v2_result;
  }

  if (get_schema_version() >= 3) {
    return core::Result<bool, std::string>::ok(true);
  }

  char* err_msg = nullptr;
  int rc = sqlite3_exec(db_.get(), kSchemaV3, nullptr, nullptr, &err_msg);
  if (rc != SQLITE_OK) {
    std::string error = err_msg != nullptr ? err_msg : "Unknown error";
    sqlite3_free(err_msg);
    return core::Result<bool, std::string>::err("Failed to apply schema v3: " + error);
  }

  return core::Result<bool, std::string>::ok(true);
}

core::Result<bool, std::string> SqliteDb::ensure_schema_v4() {
  // Ensure v3 is applied first
  auto v3_result = ensure_schema_v3();
  if (!v3_result.has_value()) {
    return v3_result;
  }

  if (get_schema_version() >= 4) {
    return core::Result<bool, std::string>::ok(true);
  }

  char* err_msg = nullptr;
  int rc = sqlite3_exec(db_.get(), kSchemaV4, nullptr, nullptr, &err_msg);
  if (rc != SQLITE_OK) {
    std::string error = err_msg != nullptr ? err_msg : "Unknown error";
    sqlite3_free(err_msg);
    return core::Result<bool, std::string>::err("Failed to apply schema v4: " + error);
  }

  return core::Result<bool, std::string>::ok(true);
}

core::Result<bool, std::string> SqliteDb::exec(const std::string& sql) {
  char* err_msg = nullptr;
  int rc = sqlite3_exec(db_.get(), sql.c_str(), nullptr, nullptr, &err_msg);
  if (rc != SQLITE_OK) {
    std::string error = err_msg != nullptr ? err_msg : "Unknown error";
    sqlite3_free(err_msg);
    return core::Result<bool, std::string>::err("SQL execution failed: " + error);
  }

  return core::Result<bool, std::string>::ok(true);
}

PreparedStatement::PreparedStatement(sqlite3* db, const std::string& sql) {
  sqlite3_stmt* raw_stmt = nullptr;
  int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &raw_stmt, nullptr);
  if (rc != SQLITE_OK) {
    error_ = sqlite3_errmsg(db);
    stmt_ = nullptr;
  } else {
    stmt_.reset(raw_stmt);
  }
}

void PreparedStatement::reset() {
  if (stmt_) {
    sqlite3_reset(stmt_.get());
    sqlite3_clear_bindings(stmt_.get());
  }
}

}  // namespace ccmcp::storage::sqlite
