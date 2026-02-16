#include "ccmcp/storage/sqlite/sqlite_audit_log.h"

#include <nlohmann/json.hpp>

#include <sqlite3.h>

namespace ccmcp::storage::sqlite {

SqliteAuditLog::SqliteAuditLog(std::shared_ptr<SqliteDb> db) : db_(std::move(db)) {}

void SqliteAuditLog::append(const AuditEvent& event) {
  int idx = get_next_index(event.trace_id);

  // Serialize refs to JSON array
  nlohmann::json refs_json = event.refs;

  const char* sql = R"(
    INSERT INTO audit_events (event_id, trace_id, event_type, payload, created_at, entity_ids_json, idx)
    VALUES (?, ?, ?, ?, ?, ?, ?)
  )";

  PreparedStatement stmt(db_->connection(), sql);
  if (!stmt.is_valid()) {
    return;
  }

  sqlite3_bind_text(stmt.get(), 1, event.event_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt.get(), 2, event.trace_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt.get(), 3, event.event_type.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt.get(), 4, event.payload.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt.get(), 5, event.created_at.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt.get(), 6, refs_json.dump().c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt.get(), 7, idx);

  sqlite3_step(stmt.get());
}

std::vector<AuditEvent> SqliteAuditLog::query(const std::string& trace_id) const {
  const char* sql = "SELECT * FROM audit_events WHERE trace_id = ? ORDER BY idx";

  PreparedStatement stmt(db_->connection(), sql);
  if (!stmt.is_valid()) {
    return {};
  }

  sqlite3_bind_text(stmt.get(), 1, trace_id.c_str(), -1, SQLITE_TRANSIENT);

  std::vector<AuditEvent> result;
  while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
    AuditEvent event;
    event.event_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 0));
    event.trace_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 1));
    event.event_type = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 2));
    event.payload = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 3));
    event.created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 4));

    // Deserialize refs JSON
    std::string refs_str = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 5));
    nlohmann::json refs_json = nlohmann::json::parse(refs_str);
    event.refs = refs_json.get<std::vector<std::string>>();

    result.push_back(event);
  }

  return result;
}

int SqliteAuditLog::get_next_index(const std::string& trace_id) {
  std::lock_guard<std::mutex> lock(mutex_);

  // Check if we've seen this trace before
  auto it = trace_indices_.find(trace_id);
  if (it != trace_indices_.end()) {
    int idx = it->second;
    it->second = idx + 1;
    return idx;
  }

  // New trace - check database for existing max index
  const char* sql = "SELECT MAX(idx) FROM audit_events WHERE trace_id = ?";
  PreparedStatement stmt(db_->connection(), sql);
  if (!stmt.is_valid()) {
    // Default to 0 if query fails
    trace_indices_[trace_id] = 1;
    return 0;
  }

  sqlite3_bind_text(stmt.get(), 1, trace_id.c_str(), -1, SQLITE_TRANSIENT);

  int max_idx = -1;
  if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
    if (sqlite3_column_type(stmt.get(), 0) != SQLITE_NULL) {
      max_idx = sqlite3_column_int(stmt.get(), 0);
    }
  }

  int next_idx = max_idx + 1;
  trace_indices_[trace_id] = next_idx + 1;
  return next_idx;
}

}  // namespace ccmcp::storage::sqlite
