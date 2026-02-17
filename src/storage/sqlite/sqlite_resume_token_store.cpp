#include "ccmcp/storage/sqlite/sqlite_resume_token_store.h"

#include "ccmcp/domain/resume_token_ir_json.h"

#include <nlohmann/json.hpp>

#include <sqlite3.h>

namespace ccmcp::storage::sqlite {

SqliteResumeTokenStore::SqliteResumeTokenStore(std::shared_ptr<SqliteDb> db) : db_(std::move(db)) {}

void SqliteResumeTokenStore::upsert(const std::string& token_ir_id, const core::ResumeId& resume_id,
                                    const domain::ResumeTokenIR& token_ir) {
  const char* sql = R"(
    INSERT INTO resume_token_ir (token_ir_id, resume_id, token_ir_json, created_at)
    VALUES (?, ?, ?, datetime('now'))
    ON CONFLICT(token_ir_id) DO UPDATE SET
      token_ir_json = excluded.token_ir_json,
      created_at = excluded.created_at
  )";

  PreparedStatement stmt(db_->connection(), sql);
  if (!stmt.is_valid()) {
    return;  // Silent failure for upsert
  }

  std::string token_ir_json = domain::token_ir_to_json_string(token_ir);

  sqlite3_bind_text(stmt.get(), 1, token_ir_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt.get(), 2, resume_id.value.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt.get(), 3, token_ir_json.c_str(), -1, SQLITE_TRANSIENT);

  sqlite3_step(stmt.get());
}

std::optional<domain::ResumeTokenIR> SqliteResumeTokenStore::get(
    const std::string& token_ir_id) const {
  const char* sql = "SELECT token_ir_json FROM resume_token_ir WHERE token_ir_id = ?";

  PreparedStatement stmt(db_->connection(), sql);
  if (!stmt.is_valid()) {
    return std::nullopt;
  }

  sqlite3_bind_text(stmt.get(), 1, token_ir_id.c_str(), -1, SQLITE_TRANSIENT);

  if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
    return row_to_token_ir(stmt.get());
  }

  return std::nullopt;
}

std::optional<domain::ResumeTokenIR> SqliteResumeTokenStore::get_by_resume(
    const core::ResumeId& resume_id) const {
  const char* sql = "SELECT token_ir_json FROM resume_token_ir WHERE resume_id = ?";

  PreparedStatement stmt(db_->connection(), sql);
  if (!stmt.is_valid()) {
    return std::nullopt;
  }

  sqlite3_bind_text(stmt.get(), 1, resume_id.value.c_str(), -1, SQLITE_TRANSIENT);

  if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
    return row_to_token_ir(stmt.get());
  }

  return std::nullopt;
}

std::vector<domain::ResumeTokenIR> SqliteResumeTokenStore::list_all() const {
  const char* sql = "SELECT token_ir_json FROM resume_token_ir ORDER BY token_ir_id";

  PreparedStatement stmt(db_->connection(), sql);
  if (!stmt.is_valid()) {
    return {};
  }

  std::vector<domain::ResumeTokenIR> results;
  while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
    results.push_back(row_to_token_ir(stmt.get()));
  }

  return results;
}

domain::ResumeTokenIR SqliteResumeTokenStore::row_to_token_ir(sqlite3_stmt* stmt) const {
  const char* json_text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
  std::string json_str = json_text != nullptr ? json_text : "{}";

  try {
    auto json_obj = nlohmann::json::parse(json_str);
    return domain::token_ir_from_json(json_obj);
  } catch (...) {
    // Return empty token IR on parse failure
    return domain::ResumeTokenIR{};
  }
}

}  // namespace ccmcp::storage::sqlite
