#include "ccmcp/storage/sqlite/sqlite_interaction_repository.h"

#include <sqlite3.h>

namespace ccmcp::storage::sqlite {

SqliteInteractionRepository::SqliteInteractionRepository(std::shared_ptr<SqliteDb> db)
    : db_(std::move(db)) {}

void SqliteInteractionRepository::upsert(const domain::Interaction& interaction) {
  const char* sql = R"(
    INSERT INTO interactions (interaction_id, contact_id, opportunity_id, state)
    VALUES (?, ?, ?, ?)
    ON CONFLICT(interaction_id) DO UPDATE SET
      contact_id = excluded.contact_id,
      opportunity_id = excluded.opportunity_id,
      state = excluded.state
  )";

  PreparedStatement stmt(db_->connection(), sql);
  if (!stmt.is_valid()) {
    return;
  }

  sqlite3_bind_text(stmt.get(), 1, interaction.interaction_id.value.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt.get(), 2, interaction.contact_id.value.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt.get(), 3, interaction.opportunity_id.value.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt.get(), 4, static_cast<int>(interaction.state));

  sqlite3_step(stmt.get());
}

std::optional<domain::Interaction> SqliteInteractionRepository::get(
    const core::InteractionId& id) const {
  const char* sql = "SELECT * FROM interactions WHERE interaction_id = ?";

  PreparedStatement stmt(db_->connection(), sql);
  if (!stmt.is_valid()) {
    return std::nullopt;
  }

  sqlite3_bind_text(stmt.get(), 1, id.value.c_str(), -1, SQLITE_TRANSIENT);

  if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
    return row_to_interaction(stmt.get());
  }

  return std::nullopt;
}

std::vector<domain::Interaction> SqliteInteractionRepository::list_by_opportunity(
    const core::OpportunityId& id) const {
  const char* sql = "SELECT * FROM interactions WHERE opportunity_id = ? ORDER BY interaction_id";

  PreparedStatement stmt(db_->connection(), sql);
  if (!stmt.is_valid()) {
    return {};
  }

  sqlite3_bind_text(stmt.get(), 1, id.value.c_str(), -1, SQLITE_TRANSIENT);

  std::vector<domain::Interaction> result;
  while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
    result.push_back(row_to_interaction(stmt.get()));
  }

  return result;
}

std::vector<domain::Interaction> SqliteInteractionRepository::list_all() const {
  const char* sql = "SELECT * FROM interactions ORDER BY interaction_id";

  PreparedStatement stmt(db_->connection(), sql);
  if (!stmt.is_valid()) {
    return {};
  }

  std::vector<domain::Interaction> result;
  while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
    result.push_back(row_to_interaction(stmt.get()));
  }

  return result;
}

domain::Interaction SqliteInteractionRepository::row_to_interaction(sqlite3_stmt* stmt) const {
  domain::Interaction interaction;

  interaction.interaction_id =
      core::InteractionId{reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0))};
  interaction.contact_id =
      core::ContactId{reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1))};
  interaction.opportunity_id =
      core::OpportunityId{reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2))};
  interaction.state = static_cast<domain::InteractionState>(sqlite3_column_int(stmt, 3));

  return interaction;
}

}  // namespace ccmcp::storage::sqlite
