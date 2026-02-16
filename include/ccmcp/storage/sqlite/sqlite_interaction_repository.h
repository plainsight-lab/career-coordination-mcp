#pragma once

#include "ccmcp/storage/repositories.h"
#include "ccmcp/storage/sqlite/sqlite_db.h"

#include <memory>

namespace ccmcp::storage::sqlite {

// SqliteInteractionRepository implements IInteractionRepository with SQLite backend.
// Stores interaction state as INTEGER.
class SqliteInteractionRepository final : public IInteractionRepository {
 public:
  explicit SqliteInteractionRepository(std::shared_ptr<SqliteDb> db);

  void upsert(const domain::Interaction& interaction) override;
  [[nodiscard]] std::optional<domain::Interaction> get(
      const core::InteractionId& id) const override;
  [[nodiscard]] std::vector<domain::Interaction> list_by_opportunity(
      const core::OpportunityId& id) const override;
  [[nodiscard]] std::vector<domain::Interaction> list_all() const override;

 private:
  std::shared_ptr<SqliteDb> db_;

  // Helper to deserialize interaction from prepared statement row
  [[nodiscard]] domain::Interaction row_to_interaction(sqlite3_stmt* stmt) const;
};

}  // namespace ccmcp::storage::sqlite
