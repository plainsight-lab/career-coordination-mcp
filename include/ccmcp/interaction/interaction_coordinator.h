#pragma once

#include "ccmcp/core/ids.h"
#include "ccmcp/domain/interaction.h"

#include <optional>
#include <string>
#include <vector>

namespace ccmcp::interaction {

// TransitionOutcome represents the result of attempting to apply a state transition.
enum class TransitionOutcome {
  kApplied,            // Transition successfully applied
  kAlreadyApplied,     // Idempotency: same transition with same key already applied
  kConflict,           // Concurrent modification: transition_index mismatch
  kNotFound,           // Interaction does not exist
  kInvalidTransition,  // Domain validation failed (not allowed from current state)
  kBackendError,       // Redis/storage backend unavailable or error
};

// TransitionResult contains the full outcome of a transition attempt.
struct TransitionResult {
  TransitionOutcome outcome;
  domain::InteractionState before_state;
  domain::InteractionState after_state;
  int64_t transition_index{0};  // Monotonic counter for this interaction
  std::string error_message;    // Populated for kBackendError or other failures
};

// IInteractionCoordinator manages atomic, idempotent state transitions for Interactions.
//
// Responsibilities:
// - Ensure only one transition succeeds when multiple workers race
// - Detect and reject replays of the same idempotency key
// - Validate transitions using domain logic (Interaction::can_transition/apply)
// - Track monotonic transition_index for optimistic concurrency control
//
// Design principles:
// - Domain validation (can_transition, apply) remains in domain code
// - Coordinator layer provides atomicity + idempotency guarantees
// - Separation from IInteractionRepository (which handles persistence)
class IInteractionCoordinator {
 public:
  virtual ~IInteractionCoordinator() = default;

  // apply_transition attempts to apply an event to an interaction.
  //
  // Parameters:
  // - interaction_id: Target interaction
  // - event: Transition event to apply
  // - idempotency_key: Unique key for deduplication (e.g., request ID)
  //
  // Idempotency semantics:
  // - First call with key K: applies transition, returns kApplied
  // - Subsequent calls with same K: returns kAlreadyApplied with same after_state
  //
  // Concurrency semantics:
  // - Two workers with different events on same interaction:
  //   - One succeeds (kApplied)
  //   - Other gets kConflict or kInvalidTransition (depending on state after first)
  //
  // Returns:
  // - TransitionResult with outcome, states, and transition_index
  [[nodiscard]] virtual TransitionResult apply_transition(const core::InteractionId& interaction_id,
                                                          domain::InteractionEvent event,
                                                          const std::string& idempotency_key) = 0;

  // get_state retrieves the current state and transition index for an interaction.
  //
  // Returns:
  // - Optional containing {state, transition_index} if found
  // - nullopt if interaction does not exist
  struct StateInfo {
    domain::InteractionState state;
    int64_t transition_index;
  };
  [[nodiscard]] virtual std::optional<StateInfo> get_state(
      const core::InteractionId& interaction_id) const = 0;

  // create_interaction initializes a new interaction in the coordinator.
  //
  // Parameters:
  // - interaction_id: ID of new interaction
  // - contact_id: Associated contact
  // - opportunity_id: Associated opportunity
  //
  // Initial state: kDraft, transition_index = 0
  //
  // Returns:
  // - true if created
  // - false if already exists or backend error
  virtual bool create_interaction(const core::InteractionId& interaction_id,
                                  const core::ContactId& contact_id,
                                  const core::OpportunityId& opportunity_id) = 0;
};

}  // namespace ccmcp::interaction
