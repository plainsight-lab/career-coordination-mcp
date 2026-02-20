#pragma once

#ifdef CCMCP_TRANSPORT_BOUNDARY_GUARD
#error "Concrete storage/redis header included in a guarded translation unit — use interfaces only."
#endif

#include "ccmcp/interaction/interaction_coordinator.h"

#include <memory>
#include <string>

// Forward declare Redis++ types to avoid exposing them in header
namespace sw {
namespace redis {
class Redis;
}
}  // namespace sw

namespace ccmcp::interaction {

// RedisInteractionCoordinator provides Redis-backed atomic, idempotent coordination.
//
// Redis data model:
// - State: ccmcp:interaction:{id}:state (hash)
//   - Fields: state (int), transition_index (int), contact_id (str), opportunity_id (str)
// - Idempotency: ccmcp:interaction:{id}:idem:{key} (hash)
//   - Fields: after_state (int), transition_index (int), applied_event (int)
//   - TTL: configurable (default: no TTL for determinism)
//
// Atomicity: Lua script ensures CAS on transition_index + idempotency check + state update
//
// Design:
// - Validates transitions using domain Interaction::can_transition
// - Uses Lua script for atomic read-check-write
// - TTL configurable but defaults to no expiration
class RedisInteractionCoordinator final : public IInteractionCoordinator {
 public:
  // Construct with Redis connection string (e.g., "tcp://127.0.0.1:6379")
  // Throws std::runtime_error if connection fails
  explicit RedisInteractionCoordinator(const std::string& redis_uri);

  ~RedisInteractionCoordinator() override;

  // Disable copy/move (unique_ptr to implementation)
  RedisInteractionCoordinator(const RedisInteractionCoordinator&) = delete;
  RedisInteractionCoordinator& operator=(const RedisInteractionCoordinator&) = delete;
  RedisInteractionCoordinator(RedisInteractionCoordinator&&) = delete;
  RedisInteractionCoordinator& operator=(RedisInteractionCoordinator&&) = delete;

  [[nodiscard]] TransitionResult apply_transition(const core::InteractionId& interaction_id,
                                                  domain::InteractionEvent event,
                                                  const std::string& idempotency_key) override;

  [[nodiscard]] std::optional<StateInfo> get_state(
      const core::InteractionId& interaction_id) const override;

  bool create_interaction(const core::InteractionId& interaction_id,
                          const core::ContactId& contact_id,
                          const core::OpportunityId& opportunity_id) override;

 private:
  std::unique_ptr<sw::redis::Redis> redis_;

  // Lua script SHA for atomic transition application
  std::string apply_transition_script_sha_;

  // Load Lua scripts into Redis
  void load_scripts();
};

}  // namespace ccmcp::interaction
