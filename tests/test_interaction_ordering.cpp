#include "ccmcp/interaction/inmemory_interaction_coordinator.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <string>
#include <vector>

using namespace ccmcp;

// ── Monotonic transition_index ──────────────────────────────────────────────

TEST_CASE("InMemoryInteractionCoordinator: transition_index increases monotonically",
          "[interaction][coordinator][ordering]") {
  interaction::InMemoryInteractionCoordinator coordinator;

  core::InteractionId id{"int-ordering-001"};
  coordinator.create_interaction(id, core::ContactId{"c"}, core::OpportunityId{"o"});

  // Initial state: transition_index == 0
  auto initial = coordinator.get_state(id);
  REQUIRE(initial.has_value());
  CHECK(initial->transition_index == 0);

  // Apply the full valid sequence: kDraft → kReady → kSent → kResponded → kClosed
  const std::vector<domain::InteractionEvent> events = {
      domain::InteractionEvent::kPrepare,
      domain::InteractionEvent::kSend,
      domain::InteractionEvent::kReceiveReply,
      domain::InteractionEvent::kClose,
  };

  int64_t prev_index = 0;
  for (std::size_t i = 0; i < events.size(); ++i) {
    const auto key = "idem-ordering-" + std::to_string(i);
    const auto result = coordinator.apply_transition(id, events[i], key);

    REQUIRE(result.outcome == interaction::TransitionOutcome::kApplied);
    // transition_index must strictly increase on every successful application.
    CHECK(result.transition_index > prev_index);
    // transition_index must equal the sequential step number (1-indexed).
    CHECK(result.transition_index == static_cast<int64_t>(i + 1));
    prev_index = result.transition_index;
  }

  // Final state: transition_index == number of transitions applied
  auto final_state = coordinator.get_state(id);
  REQUIRE(final_state.has_value());
  CHECK(final_state->transition_index == static_cast<int64_t>(events.size()));
}

// ── Idempotency receipt preserves transition_index ──────────────────────────

TEST_CASE(
    "InMemoryInteractionCoordinator: idempotency receipt preserves transition_index regardless "
    "of call order",
    "[interaction][coordinator][ordering]") {
  interaction::InMemoryInteractionCoordinator coordinator;

  core::InteractionId id{"int-ordering-002"};
  coordinator.create_interaction(id, core::ContactId{"c"}, core::OpportunityId{"o"});

  const std::string idem_key = "idem-idempotency-001";

  // First apply: transition_index == 1
  auto r1 = coordinator.apply_transition(id, domain::InteractionEvent::kPrepare, idem_key);
  REQUIRE(r1.outcome == interaction::TransitionOutcome::kApplied);
  const int64_t original_index = r1.transition_index;
  CHECK(original_index == 1);

  // Replay with the same key: must return the exact same transition_index.
  auto r2 = coordinator.apply_transition(id, domain::InteractionEvent::kPrepare, idem_key);
  REQUIRE(r2.outcome == interaction::TransitionOutcome::kAlreadyApplied);
  CHECK(r2.transition_index == original_index);

  // Replay again: still the same transition_index.
  auto r3 = coordinator.apply_transition(id, domain::InteractionEvent::kPrepare, idem_key);
  REQUIRE(r3.outcome == interaction::TransitionOutcome::kAlreadyApplied);
  CHECK(r3.transition_index == original_index);

  // Coordinator state must not have advanced beyond the original application.
  auto state = coordinator.get_state(id);
  REQUIRE(state.has_value());
  CHECK(state->transition_index == original_index);
}
