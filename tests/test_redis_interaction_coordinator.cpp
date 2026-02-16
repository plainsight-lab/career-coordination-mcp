#include "ccmcp/interaction/redis_interaction_coordinator.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdlib>

using namespace ccmcp;

// Helper: Check if Redis integration tests should run
static bool should_run_redis_tests() {
  const char* env = std::getenv("CCMCP_TEST_REDIS");
  return env != nullptr && std::string(env) == "1";
}

// Helper: Get Redis URI from environment or use default
static std::string get_redis_uri() {
  const char* env = std::getenv("CCMCP_REDIS_URI");
  if (env != nullptr) {
    return std::string(env);
  }
  return "tcp://127.0.0.1:6379";
}

TEST_CASE("RedisInteractionCoordinator: idempotency - same key returns AlreadyApplied",
          "[interaction][coordinator][redis][integration]") {
  if (!should_run_redis_tests()) {
    SKIP("Redis integration tests disabled (set CCMCP_TEST_REDIS=1 to enable)");
  }

  try {
    interaction::RedisInteractionCoordinator coordinator(get_redis_uri());

    core::InteractionId id{"redis-int-idem-001"};
    core::ContactId contact{"contact-001"};
    core::OpportunityId opportunity{"opp-001"};

    // Create interaction
    bool created = coordinator.create_interaction(id, contact, opportunity);
    REQUIRE(created);

    const std::string idem_key = "idem-redis-unique-123";

    // First application: should succeed
    auto result1 = coordinator.apply_transition(id, domain::InteractionEvent::kPrepare, idem_key);
    REQUIRE(result1.outcome == interaction::TransitionOutcome::kApplied);
    CHECK(result1.before_state == domain::InteractionState::kDraft);
    CHECK(result1.after_state == domain::InteractionState::kReady);
    CHECK(result1.transition_index == 1);

    // Second application with same idempotency key: should return AlreadyApplied
    auto result2 = coordinator.apply_transition(id, domain::InteractionEvent::kPrepare, idem_key);
    REQUIRE(result2.outcome == interaction::TransitionOutcome::kAlreadyApplied);
    CHECK(result2.before_state == domain::InteractionState::kReady);
    CHECK(result2.after_state == domain::InteractionState::kReady);
    CHECK(result2.transition_index == 1);  // Same index as first application

    // Verify state is still kReady with transition_index=1
    auto state = coordinator.get_state(id);
    REQUIRE(state.has_value());
    CHECK(state->state == domain::InteractionState::kReady);
    CHECK(state->transition_index == 1);

  } catch (const std::runtime_error& e) {
    FAIL("Redis connection failed: " << e.what());
  }
}

TEST_CASE(
    "RedisInteractionCoordinator: concurrent transitions - one succeeds, other detects invalid",
    "[interaction][coordinator][redis][integration]") {
  if (!should_run_redis_tests()) {
    SKIP("Redis integration tests disabled (set CCMCP_TEST_REDIS=1 to enable)");
  }

  try {
    interaction::RedisInteractionCoordinator coordinator(get_redis_uri());

    core::InteractionId id{"redis-int-concurrent-001"};
    core::ContactId contact{"contact-001"};
    core::OpportunityId opportunity{"opp-001"};

    // Create interaction
    bool created = coordinator.create_interaction(id, contact, opportunity);
    REQUIRE(created);

    // Simulate two workers attempting different transitions from kDraft
    // Worker 1: kPrepare (valid from kDraft)
    // Worker 2: kSend (invalid from kDraft, only valid from kReady)

    // Worker 1 applies kPrepare
    auto result1 = coordinator.apply_transition(id, domain::InteractionEvent::kPrepare, "worker-1");
    REQUIRE(result1.outcome == interaction::TransitionOutcome::kApplied);
    CHECK(result1.after_state == domain::InteractionState::kReady);

    // Worker 2 attempts kSend (now invalid because state is kReady, not kDraft)
    // From kReady, kSend is valid, so this would succeed
    // To test conflict, we need a transition that's invalid from kReady

    // Better test: Worker 2 tries kPrepare again (invalid from kReady)
    auto result2 = coordinator.apply_transition(id, domain::InteractionEvent::kPrepare, "worker-2");
    REQUIRE(result2.outcome == interaction::TransitionOutcome::kInvalidTransition);
    CHECK(result2.before_state == domain::InteractionState::kReady);
    CHECK(result2.after_state == domain::InteractionState::kReady);

    // Verify final state is kReady (Worker 1's transition)
    auto state = coordinator.get_state(id);
    REQUIRE(state.has_value());
    CHECK(state->state == domain::InteractionState::kReady);
    CHECK(state->transition_index == 1);

  } catch (const std::runtime_error& e) {
    FAIL("Redis connection failed: " << e.what());
  }
}

TEST_CASE("RedisInteractionCoordinator: valid transition sequence",
          "[interaction][coordinator][redis][integration]") {
  if (!should_run_redis_tests()) {
    SKIP("Redis integration tests disabled (set CCMCP_TEST_REDIS=1 to enable)");
  }

  try {
    interaction::RedisInteractionCoordinator coordinator(get_redis_uri());

    core::InteractionId id{"redis-int-sequence-001"};
    coordinator.create_interaction(id, core::ContactId{"contact"}, core::OpportunityId{"opp"});

    // kDraft -> kPrepare -> kReady
    auto r1 = coordinator.apply_transition(id, domain::InteractionEvent::kPrepare, "step-1");
    REQUIRE(r1.outcome == interaction::TransitionOutcome::kApplied);
    CHECK(r1.after_state == domain::InteractionState::kReady);
    CHECK(r1.transition_index == 1);

    // kReady -> kSend -> kSent
    auto r2 = coordinator.apply_transition(id, domain::InteractionEvent::kSend, "step-2");
    REQUIRE(r2.outcome == interaction::TransitionOutcome::kApplied);
    CHECK(r2.after_state == domain::InteractionState::kSent);
    CHECK(r2.transition_index == 2);

    // kSent -> kReceiveReply -> kResponded
    auto r3 = coordinator.apply_transition(id, domain::InteractionEvent::kReceiveReply, "step-3");
    REQUIRE(r3.outcome == interaction::TransitionOutcome::kApplied);
    CHECK(r3.after_state == domain::InteractionState::kResponded);
    CHECK(r3.transition_index == 3);

    // Verify final state
    auto state = coordinator.get_state(id);
    REQUIRE(state.has_value());
    CHECK(state->state == domain::InteractionState::kResponded);
    CHECK(state->transition_index == 3);

  } catch (const std::runtime_error& e) {
    FAIL("Redis connection failed: " << e.what());
  }
}

TEST_CASE("RedisInteractionCoordinator: invalid transition rejected",
          "[interaction][coordinator][redis][integration]") {
  if (!should_run_redis_tests()) {
    SKIP("Redis integration tests disabled (set CCMCP_TEST_REDIS=1 to enable)");
  }

  try {
    interaction::RedisInteractionCoordinator coordinator(get_redis_uri());

    core::InteractionId id{"redis-int-invalid-001"};
    coordinator.create_interaction(id, core::ContactId{"contact"}, core::OpportunityId{"opp"});

    // Try invalid transition: kDraft -> kSend (not allowed, must kPrepare first)
    auto result = coordinator.apply_transition(id, domain::InteractionEvent::kSend, "idem-001");

    REQUIRE(result.outcome == interaction::TransitionOutcome::kInvalidTransition);
    CHECK(result.before_state == domain::InteractionState::kDraft);
    CHECK(result.after_state == domain::InteractionState::kDraft);
    CHECK(result.transition_index == 0);

    // Verify state unchanged
    auto state = coordinator.get_state(id);
    REQUIRE(state.has_value());
    CHECK(state->state == domain::InteractionState::kDraft);
    CHECK(state->transition_index == 0);

  } catch (const std::runtime_error& e) {
    FAIL("Redis connection failed: " << e.what());
  }
}
