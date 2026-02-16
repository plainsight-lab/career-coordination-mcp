#pragma once

#include "ccmcp/storage/repositories.h"

#include <map>

namespace ccmcp::storage {

// InMemoryOpportunityRepository stores Opportunities in-memory using std::map.
// std::map guarantees deterministic iteration order (sorted by OpportunityId).
// Suitable for testing and v0.2 development; will be replaced with SQLite in later slices.
class InMemoryOpportunityRepository final : public IOpportunityRepository {
 public:
  void upsert(const domain::Opportunity& opportunity) override;
  [[nodiscard]] std::optional<domain::Opportunity> get(
      const core::OpportunityId& id) const override;
  [[nodiscard]] std::vector<domain::Opportunity> list_all() const override;

 private:
  std::map<core::OpportunityId, domain::Opportunity> opportunities_;
};

}  // namespace ccmcp::storage
