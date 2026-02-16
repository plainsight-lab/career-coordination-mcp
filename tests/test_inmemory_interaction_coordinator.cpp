#include "ccmcp/interaction/inmemory_interaction_coordinator.h"

#include <catch2/catch_test_macros.hpp>

using namespace ccmcp;

TEST_CASE("InMemoryInteractionCoordinator: create and get state", "[interaction][coordinator]") {
  interaction::InMemoryInteractionCoordinator coordinator;

  core::InteractionId id{"int-001"};
  core::ContactId contact{"contact-001"};
  core::OpportunityId opportunity{"opp-001"};

  // Initially, interaction does not exist
  auto state_before = coordinator.get_state(id);
  CHECK_FALSE(state_before.has_value());

  // Create interaction
  bool created = coordinator.create_interaction(id, contact, opportunity);
  REQUIRE(created);

  // Now state should exist in kDraft
  auto state_after = coordinator.get_state(id);
  REQUIRE(state_after.has_value());
  CHECK(state_after->state == domain::InteractionState::kDraft);
  CHECK(state_after->transition_index == 0);

  // Creating again should fail
  bool created_again = coordinator.create_interaction(id, contact, opportunity);
  CHECK_FALSE(created_again);
}

TEST_CASE("InMemoryInteractionCoordinator: valid transition", "[interaction][coordinator]") {
  interaction::InMemoryInteractionCoordinator coordinator;

  core::InteractionId id{"int-001"};
  coordinator.create_interaction(id, core::ContactId{"contact"}, core::OpportunityId{"opp"});

  // Apply valid transition: kDraft -> kPrepare -> kReady
  auto result = coordinator.apply_transition(id, domain::InteractionEvent::kPrepare, "idem-001");

  REQUIRE(result.outcome == interaction::TransitionOutcome::kApplied);
  CHECK(result.before_state == domain::InteractionState::kDraft);
  CHECK(result.after_state == domain::InteractionState::kReady);
  CHECK(result.transition_index == 1);
  CHECK(result.error_message.empty());

  // Verify state updated
  auto state = coordinator.get_state(id);
  REQUIRE(state.has_value());
  CHECK(state->state == domain::InteractionState::kReady);
  CHECK(state->transition_index == 1);
}

TEST_CASE("InMemoryInteractionCoordinator: invalid transition", "[interaction][coordinator]") {
  interaction::InMemoryInteractionCoordinator coordinator;

  core::InteractionId id{"int-001"};
  coordinator.create_interaction(id, core::ContactId{"contact"}, core::OpportunityId{"opp"});

  // Try invalid transition: kDraft -> kSend (not allowed, must kPrepare first)
  auto result = coordinator.apply_transition(id, domain::InteractionEvent::kSend, "idem-001");

  REQUIRE(result.outcome == interaction::TransitionOutcome::kInvalidTransition);
  CHECK(result.before_state == domain::InteractionState::kDraft);
  CHECK(result.after_state == domain::InteractionState::kDraft);  // State unchanged
  CHECK(result.transition_index == 0);                            // Index unchanged
  CHECK_FALSE(result.error_message.empty());

  // Verify state unchanged
  auto state = coordinator.get_state(id);
  REQUIRE(state.has_value());
  CHECK(state->state == domain::InteractionState::kDraft);
  CHECK(state->transition_index == 0);
}

TEST_CASE("InMemoryInteractionCoordinator: idempotency - same key returns AlreadyApplied",
          "[interaction][coordinator][idempotency]") {
  interaction::InMemoryInteractionCoordinator coordinator;

  core::InteractionId id{"int-001"};
  coordinator.create_interaction(id, core::ContactId{"contact"}, core::OpportunityId{"opp"});

  const std::string idem_key = "idem-unique-123";

  // First application: should succeed
  auto result1 = coordinator.apply_transition(id, domain::InteractionEvent::kPrepare, idem_key);
  REQUIRE(result1.outcome == interaction::TransitionOutcome::kApplied);
  CHECK(result1.after_state == domain::InteractionState::kReady);
  CHECK(result1.transition_index == 1);

  // Second application with same idempotency key: should return AlreadyApplied
  auto result2 = coordinator.apply_transition(id, domain::InteractionEvent::kPrepare, idem_key);
  REQUIRE(result2.outcome == interaction::TransitionOutcome::kAlreadyApplied);
  CHECK(result2.before_state ==
        domain::InteractionState::kReady);  // Already transitioned, before=after
  CHECK(result2.after_state == domain::InteractionState::kReady);
  CHECK(result2.transition_index == 1);  // Same index as first application

  // Verify state is still kReady with transition_index=1
  auto state = coordinator.get_state(id);
  REQUIRE(state.has_value());
  CHECK(state->state == domain::InteractionState::kReady);
  CHECK(state->transition_index == 1);
}

TEST_CASE("InMemoryInteractionCoordinator: idempotency - different keys both apply",
          "[interaction][coordinator][idempotency]") {
  interaction::InMemoryInteractionCoordinator coordinator;

  core::InteractionId id{"int-001"};
  coordinator.create_interaction(id, core::ContactId{"contact"}, core::OpportunityId{"opp"});

  // Apply transition with key A
  auto result1 = coordinator.apply_transition(id, domain::InteractionEvent::kPrepare, "idem-A");
  REQUIRE(result1.outcome == interaction::TransitionOutcome::kApplied);
  CHECK(result1.after_state == domain::InteractionState::kReady);
  CHECK(result1.transition_index == 1);

  // Apply next transition with key B (different event)
  auto result2 = coordinator.apply_transition(id, domain::InteractionEvent::kSend, "idem-B");
  REQUIRE(result2.outcome == interaction::TransitionOutcome::kApplied);
  CHECK(result2.before_state == domain::InteractionState::kReady);
  CHECK(result2.after_state == domain::InteractionState::kSent);
  CHECK(result2.transition_index == 2);

  // Verify final state
  auto state = coordinator.get_state(id);
  REQUIRE(state.has_value());
  CHECK(state->state == domain::InteractionState::kSent);
  CHECK(state->transition_index == 2);
}

TEST_CASE("InMemoryInteractionCoordinator: transition on non-existent interaction",
          "[interaction][coordinator]") {
  interaction::InMemoryInteractionCoordinator coordinator;

  core::InteractionId id{"int-nonexistent"};

  // Try to apply transition on non-existent interaction
  auto result = coordinator.apply_transition(id, domain::InteractionEvent::kPrepare, "idem-001");

  REQUIRE(result.outcome == interaction::TransitionOutcome::kNotFound);
  CHECK_FALSE(result.error_message.empty());
}

TEST_CASE("InMemoryInteractionCoordinator: full state machine lifecycle",
          "[interaction][coordinator]") {
  interaction::InMemoryInteractionCoordinator coordinator;

  core::InteractionId id{"int-001"};
  coordinator.create_interaction(id, core::ContactId{"contact"}, core::OpportunityId{"opp"});

  // kDraft -> kPrepare -> kReady
  auto r1 = coordinator.apply_transition(id, domain::InteractionEvent::kPrepare, "step-1");
  REQUIRE(r1.outcome == interaction::TransitionOutcome::kApplied);
  CHECK(r1.after_state == domain::InteractionState::kReady);

  // kReady -> kSend -> kSent
  auto r2 = coordinator.apply_transition(id, domain::InteractionEvent::kSend, "step-2");
  REQUIRE(r2.outcome == interaction::TransitionOutcome::kApplied);
  CHECK(r2.after_state == domain::InteractionState::kSent);

  // kSent -> kReceiveReply -> kResponded
  auto r3 = coordinator.apply_transition(id, domain::InteractionEvent::kReceiveReply, "step-3");
  REQUIRE(r3.outcome == interaction::TransitionOutcome::kApplied);
  CHECK(r3.after_state == domain::InteractionState::kResponded);

  // kResponded -> kClose -> kClosed
  auto r4 = coordinator.apply_transition(id, domain::InteractionEvent::kClose, "step-4");
  REQUIRE(r4.outcome == interaction::TransitionOutcome::kApplied);
  CHECK(r4.after_state == domain::InteractionState::kClosed);

  // Verify final state
  auto state = coordinator.get_state(id);
  REQUIRE(state.has_value());
  CHECK(state->state == domain::InteractionState::kClosed);
  CHECK(state->transition_index == 4);

  // kClosed cannot transition further
  auto r5 = coordinator.apply_transition(id, domain::InteractionEvent::kClose, "step-5");
  REQUIRE(r5.outcome == interaction::TransitionOutcome::kInvalidTransition);
}
