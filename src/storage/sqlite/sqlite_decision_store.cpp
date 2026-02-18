#include "ccmcp/storage/sqlite/sqlite_decision_store.h"

#include "ccmcp/domain/decision_record.h"

#include <sqlite3.h>

namespace ccmcp::storage::sqlite {

SqliteDecisionStore::SqliteDecisionStore(std::shared_ptr<SqliteDb> db) : db_(std::move(db)) {}

void SqliteDecisionStore::upsert(const domain::DecisionRecord& record) {
  const char* sql = R"(
    INSERT INTO decision_records
      (decision_id, trace_id, opportunity_id, artifact_id, decision_json, created_at)
    VALUES (?, ?, ?, ?, ?, ?)
    ON CONFLICT(decision_id) DO UPDATE SET
      trace_id       = excluded.trace_id,
      opportunity_id = excluded.opportunity_id,
      artifact_id    = excluded.artifact_id,
      decision_json  = excluded.decision_json,
      created_at     = excluded.created_at
  )";

  PreparedStatement stmt(db_->connection(), sql);
  if (!stmt.is_valid()) {
    return;
  }

  const std::string json_str = domain::decision_record_to_json(record).dump();

  sqlite3_bind_text(stmt.get(), 1, record.decision_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt.get(), 2, record.trace_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt.get(), 3, record.opportunity_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt.get(), 4, record.artifact_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt.get(), 5, json_str.c_str(), -1, SQLITE_TRANSIENT);

  if (record.created_at.has_value()) {
    sqlite3_bind_text(stmt.get(), 6, record.created_at.value().c_str(), -1, SQLITE_TRANSIENT);
  } else {
    sqlite3_bind_null(stmt.get(), 6);
  }

  sqlite3_step(stmt.get());
}

std::optional<domain::DecisionRecord> SqliteDecisionStore::get(
    const std::string& decision_id) const {
  const char* sql =
      "SELECT decision_id, trace_id, opportunity_id, artifact_id, decision_json, created_at "
      "FROM decision_records WHERE decision_id = ?";

  PreparedStatement stmt(db_->connection(), sql);
  if (!stmt.is_valid()) {
    return std::nullopt;
  }

  sqlite3_bind_text(stmt.get(), 1, decision_id.c_str(), -1, SQLITE_TRANSIENT);

  if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
    return row_to_record(stmt.get());
  }

  return std::nullopt;
}

std::vector<domain::DecisionRecord> SqliteDecisionStore::list_by_trace(
    const std::string& trace_id) const {
  const char* sql =
      "SELECT decision_id, trace_id, opportunity_id, artifact_id, decision_json, created_at "
      "FROM decision_records WHERE trace_id = ? ORDER BY decision_id";

  PreparedStatement stmt(db_->connection(), sql);
  if (!stmt.is_valid()) {
    return {};
  }

  sqlite3_bind_text(stmt.get(), 1, trace_id.c_str(), -1, SQLITE_TRANSIENT);

  std::vector<domain::DecisionRecord> result;
  while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
    result.push_back(row_to_record(stmt.get()));
  }

  return result;
}

// Column order: decision_id(0), trace_id(1), opportunity_id(2),
//               artifact_id(3), decision_json(4), created_at(5)
domain::DecisionRecord SqliteDecisionStore::row_to_record(sqlite3_stmt* stmt) const {
  // Parse the full record from the stored JSON blob.
  const std::string json_str =
      reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));  // NOLINT

  domain::DecisionRecord record =
      domain::decision_record_from_json(nlohmann::json::parse(json_str));

  // Overwrite created_at from its dedicated nullable column (may differ from JSON if null).
  if (sqlite3_column_type(stmt, 5) != SQLITE_NULL) {
    record.created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));  // NOLINT
  } else {
    record.created_at = std::nullopt;
  }

  return record;
}

}  // namespace ccmcp::storage::sqlite
