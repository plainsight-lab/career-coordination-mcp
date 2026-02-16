#pragma once

#include "ccmcp/storage/repositories.h"

#include <map>

namespace ccmcp::storage {

// InMemoryInteractionRepository stores Interactions in-memory using std::map.
// std::map guarantees deterministic iteration order (sorted by InteractionId).
// Suitable for testing and v0.2 development; will be replaced with SQLite in later slices.
class InMemoryInteractionRepository final : public IInteractionRepository {
 public:
  void upsert(const domain::Interaction& interaction) override;
  [[nodiscard]] std::optional<domain::Interaction> get(
      const core::InteractionId& id) const override;
  [[nodiscard]] std::vector<domain::Interaction> list_by_opportunity(
      const core::OpportunityId& id) const override;
  [[nodiscard]] std::vector<domain::Interaction> list_all() const override;

 private:
  std::map<core::InteractionId, domain::Interaction> interactions_;
};

}  // namespace ccmcp::storage
