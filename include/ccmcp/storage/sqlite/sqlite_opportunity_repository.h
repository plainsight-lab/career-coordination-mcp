#pragma once

#ifdef CCMCP_TRANSPORT_BOUNDARY_GUARD
#error "Concrete storage/redis header included in a guarded translation unit — use interfaces only."
#endif

#include "ccmcp/storage/repositories.h"
#include "ccmcp/storage/sqlite/sqlite_db.h"

#include <memory>

namespace ccmcp::storage::sqlite {

// SqliteOpportunityRepository implements IOpportunityRepository with SQLite backend.
// Handles opportunities and requirements (one-to-many relationship).
// Requirements order is preserved via idx column.
class SqliteOpportunityRepository final : public IOpportunityRepository {
 public:
  explicit SqliteOpportunityRepository(std::shared_ptr<SqliteDb> db);

  void upsert(const domain::Opportunity& opportunity) override;
  [[nodiscard]] std::optional<domain::Opportunity> get(
      const core::OpportunityId& id) const override;
  [[nodiscard]] std::vector<domain::Opportunity> list_all() const override;

 private:
  std::shared_ptr<SqliteDb> db_;

  // Load requirements for an opportunity (ordered by idx)
  [[nodiscard]] std::vector<domain::Requirement> load_requirements(
      const core::OpportunityId& id) const;
};

}  // namespace ccmcp::storage::sqlite
