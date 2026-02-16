#pragma once

#include "ccmcp/interaction/interaction_coordinator.h"

#include <map>
#include <mutex>
#include <string>

namespace ccmcp::interaction {

// InMemoryInteractionCoordinator provides in-memory coordination for testing and development.
//
// Thread-safety: Uses std::mutex for all operations (coarse-grained locking).
// Determinism: Deterministic within single-threaded tests (no time dependencies).
//
// Design:
// - Stores Interaction state + transition_index in memory
// - Tracks idempotency receipts in separate map
// - Validates transitions using domain Interaction::can_transition/apply
class InMemoryInteractionCoordinator final : public IInteractionCoordinator {
 public:
  InMemoryInteractionCoordinator() = default;
  ~InMemoryInteractionCoordinator() override = default;

  // Disable copy/move (mutex not copyable)
  InMemoryInteractionCoordinator(const InMemoryInteractionCoordinator&) = delete;
  InMemoryInteractionCoordinator& operator=(const InMemoryInteractionCoordinator&) = delete;
  InMemoryInteractionCoordinator(InMemoryInteractionCoordinator&&) = delete;
  InMemoryInteractionCoordinator& operator=(InMemoryInteractionCoordinator&&) = delete;

  [[nodiscard]] TransitionResult apply_transition(const core::InteractionId& interaction_id,
                                                  domain::InteractionEvent event,
                                                  const std::string& idempotency_key) override;

  [[nodiscard]] std::optional<StateInfo> get_state(
      const core::InteractionId& interaction_id) const override;

  bool create_interaction(const core::InteractionId& interaction_id,
                          const core::ContactId& contact_id,
                          const core::OpportunityId& opportunity_id) override;

 private:
  struct InteractionRecord {
    domain::Interaction interaction;
    int64_t transition_index{0};
  };

  struct IdempotencyReceipt {
    domain::InteractionState after_state;
    int64_t transition_index;
    domain::InteractionEvent applied_event;
  };

  mutable std::mutex mutex_;
  std::map<std::string, InteractionRecord> interactions_;  // key: interaction_id.value
  std::map<std::string, IdempotencyReceipt>
      idempotency_receipts_;  // key: interaction_id:idempotency_key
};

}  // namespace ccmcp::interaction
