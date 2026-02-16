#include "ccmcp/storage/inmemory_interaction_repository.h"

namespace ccmcp::storage {

void InMemoryInteractionRepository::upsert(const domain::Interaction& interaction) {
  interactions_[interaction.interaction_id] = interaction;
}

std::optional<domain::Interaction> InMemoryInteractionRepository::get(
    const core::InteractionId& id) const {
  auto it = interactions_.find(id);
  if (it != interactions_.end()) {
    return it->second;
  }
  return std::nullopt;
}

std::vector<domain::Interaction> InMemoryInteractionRepository::list_by_opportunity(
    const core::OpportunityId& id) const {
  std::vector<domain::Interaction> result;
  for (const auto& [interaction_id, interaction] : interactions_) {
    if (interaction.opportunity_id == id) {
      result.push_back(interaction);
    }
  }
  return result;
}

std::vector<domain::Interaction> InMemoryInteractionRepository::list_all() const {
  std::vector<domain::Interaction> result;
  result.reserve(interactions_.size());
  for (const auto& [id, interaction] : interactions_) {
    result.push_back(interaction);
  }
  return result;
}

}  // namespace ccmcp::storage
