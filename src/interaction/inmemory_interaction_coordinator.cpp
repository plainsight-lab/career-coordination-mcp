#include "ccmcp/interaction/inmemory_interaction_coordinator.h"

#include <sstream>

namespace ccmcp::interaction {

TransitionResult InMemoryInteractionCoordinator::apply_transition(
    const core::InteractionId& interaction_id, const domain::InteractionEvent event,
    const std::string& idempotency_key) {
  std::lock_guard<std::mutex> lock(mutex_);

  const std::string int_key = interaction_id.value;
  const std::string idem_key = int_key + ":" + idempotency_key;

  // Check if interaction exists
  auto it = interactions_.find(int_key);
  if (it == interactions_.end()) {
    return TransitionResult{
        .outcome = TransitionOutcome::kNotFound,
        .before_state = domain::InteractionState::kDraft,
        .after_state = domain::InteractionState::kDraft,
        .transition_index = 0,
        .error_message = "Interaction not found: " + int_key,
    };
  }

  auto& record = it->second;

  // Check idempotency: if this key was already applied, return cached result
  auto idem_it = idempotency_receipts_.find(idem_key);
  if (idem_it != idempotency_receipts_.end()) {
    const auto& receipt = idem_it->second;
    return TransitionResult{
        .outcome = TransitionOutcome::kAlreadyApplied,
        .before_state = receipt.after_state,  // Already transitioned, before = after
        .after_state = receipt.after_state,
        .transition_index = receipt.transition_index,
        .error_message = "",
    };
  }

  // Validate transition using domain logic
  const domain::InteractionState before_state = record.interaction.state;

  if (!record.interaction.can_transition(event)) {
    return TransitionResult{
        .outcome = TransitionOutcome::kInvalidTransition,
        .before_state = before_state,
        .after_state = before_state,
        .transition_index = record.transition_index,
        .error_message = "Invalid transition from current state",
    };
  }

  // Apply transition (domain logic)
  const bool applied = record.interaction.apply(event);
  if (!applied) {
    // This should not happen if can_transition returned true, but handle defensively
    return TransitionResult{
        .outcome = TransitionOutcome::kInvalidTransition,
        .before_state = before_state,
        .after_state = before_state,
        .transition_index = record.transition_index,
        .error_message = "Failed to apply transition (domain logic rejected)",
    };
  }

  // Transition succeeded: update index and record idempotency receipt
  record.transition_index++;
  const domain::InteractionState after_state = record.interaction.state;

  idempotency_receipts_[idem_key] = IdempotencyReceipt{
      .after_state = after_state,
      .transition_index = record.transition_index,
      .applied_event = event,
  };

  return TransitionResult{
      .outcome = TransitionOutcome::kApplied,
      .before_state = before_state,
      .after_state = after_state,
      .transition_index = record.transition_index,
      .error_message = "",
  };
}

std::optional<IInteractionCoordinator::StateInfo> InMemoryInteractionCoordinator::get_state(
    const core::InteractionId& interaction_id) const {
  std::lock_guard<std::mutex> lock(mutex_);

  const std::string int_key = interaction_id.value;
  auto it = interactions_.find(int_key);
  if (it == interactions_.end()) {
    return std::nullopt;
  }

  return StateInfo{
      .state = it->second.interaction.state,
      .transition_index = it->second.transition_index,
  };
}

bool InMemoryInteractionCoordinator::create_interaction(const core::InteractionId& interaction_id,
                                                        const core::ContactId& contact_id,
                                                        const core::OpportunityId& opportunity_id) {
  std::lock_guard<std::mutex> lock(mutex_);

  const std::string int_key = interaction_id.value;

  // Check if already exists
  if (interactions_.find(int_key) != interactions_.end()) {
    return false;
  }

  // Create new interaction in kDraft state
  domain::Interaction interaction{
      .interaction_id = interaction_id,
      .contact_id = contact_id,
      .opportunity_id = opportunity_id,
      .state = domain::InteractionState::kDraft,
  };

  interactions_[int_key] = InteractionRecord{
      .interaction = interaction,
      .transition_index = 0,
  };

  return true;
}

}  // namespace ccmcp::interaction
