#include "ccmcp/storage/sqlite/sqlite_audit_log.h"

#include "ccmcp/storage/audit_chain.h"

#include <nlohmann/json.hpp>

#include <sqlite3.h>

namespace ccmcp::storage::sqlite {

SqliteAuditLog::SqliteAuditLog(std::shared_ptr<SqliteDb> db) : db_(std::move(db)) {}

void SqliteAuditLog::append(const AuditEvent& event) {
  const AppendState state = get_append_state(event.trace_id);

  const std::string event_hash = compute_event_hash(event, state.previous_hash);

  // Serialize refs to JSON array
  nlohmann::json refs_json = event.refs;

  const char* sql = R"(
    INSERT INTO audit_events
      (event_id, trace_id, event_type, payload, created_at, entity_ids_json, idx,
       previous_hash, event_hash)
    VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
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
  sqlite3_bind_int(stmt.get(), 7, state.idx);
  sqlite3_bind_text(stmt.get(), 8, state.previous_hash.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt.get(), 9, event_hash.c_str(), -1, SQLITE_TRANSIENT);

  sqlite3_step(stmt.get());
}

std::vector<AuditEvent> SqliteAuditLog::query(const std::string& trace_id) const {
  const char* sql =
      "SELECT event_id, trace_id, event_type, payload, created_at, entity_ids_json,"
      "       previous_hash, event_hash"
      "  FROM audit_events WHERE trace_id = ? ORDER BY idx";

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

    event.previous_hash =
        reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 6));                 // NOLINT
    event.event_hash = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 7));  // NOLINT

    result.push_back(event);
  }

  return result;
}

SqliteAuditLog::AppendState SqliteAuditLog::get_append_state(const std::string& trace_id) {
  std::lock_guard<std::mutex> lock(mutex_);

  // ── idx ──────────────────────────────────────────────────────────────────
  int idx = 0;
  auto it = trace_indices_.find(trace_id);
  if (it != trace_indices_.end()) {
    idx = it->second;
    it->second = idx + 1;
  } else {
    // New trace: query DB for existing max index
    const char* idx_sql = "SELECT MAX(idx) FROM audit_events WHERE trace_id = ?";
    PreparedStatement idx_stmt(db_->connection(), idx_sql);
    int max_idx = -1;
    if (idx_stmt.is_valid()) {
      sqlite3_bind_text(idx_stmt.get(), 1, trace_id.c_str(), -1, SQLITE_TRANSIENT);
      if (sqlite3_step(idx_stmt.get()) == SQLITE_ROW) {
        if (sqlite3_column_type(idx_stmt.get(), 0) != SQLITE_NULL) {
          max_idx = sqlite3_column_int(idx_stmt.get(), 0);
        }
      }
    }
    idx = max_idx + 1;
    trace_indices_[trace_id] = idx + 1;
  }

  // ── previous_hash ─────────────────────────────────────────────────────────
  std::string previous_hash = std::string(kGenesisHash);
  if (idx > 0) {
    const char* hash_sql =
        "SELECT event_hash FROM audit_events WHERE trace_id = ? ORDER BY idx DESC LIMIT 1";
    PreparedStatement hash_stmt(db_->connection(), hash_sql);
    if (hash_stmt.is_valid()) {
      sqlite3_bind_text(hash_stmt.get(), 1, trace_id.c_str(), -1, SQLITE_TRANSIENT);
      if (sqlite3_step(hash_stmt.get()) == SQLITE_ROW) {
        const auto* raw = sqlite3_column_text(hash_stmt.get(), 0);  // NOLINT
        if (raw != nullptr) {
          previous_hash = reinterpret_cast<const char*>(raw);  // NOLINT
        }
      }
    }
  }

  return {idx, std::move(previous_hash)};
}

}  // namespace ccmcp::storage::sqlite
