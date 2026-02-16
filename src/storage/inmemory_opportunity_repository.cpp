#include "ccmcp/storage/inmemory_opportunity_repository.h"

namespace ccmcp::storage {

void InMemoryOpportunityRepository::upsert(const domain::Opportunity& opportunity) {
  opportunities_[opportunity.opportunity_id] = opportunity;
}

std::optional<domain::Opportunity> InMemoryOpportunityRepository::get(
    const core::OpportunityId& id) const {
  auto it = opportunities_.find(id);
  if (it != opportunities_.end()) {
    return it->second;
  }
  return std::nullopt;
}

std::vector<domain::Opportunity> InMemoryOpportunityRepository::list_all() const {
  std::vector<domain::Opportunity> result;
  result.reserve(opportunities_.size());
  for (const auto& [id, opportunity] : opportunities_) {
    result.push_back(opportunity);
  }
  return result;
}

}  // namespace ccmcp::storage
