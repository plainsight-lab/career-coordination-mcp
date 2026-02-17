#include "ccmcp/app/app_service.h"
#include "ccmcp/core/clock.h"
#include "ccmcp/core/id_generator.h"
#include "ccmcp/embedding/embedding_provider.h"
#include "ccmcp/interaction/inmemory_interaction_coordinator.h"
#include "ccmcp/storage/audit_log.h"
#include "ccmcp/storage/inmemory_atom_repository.h"
#include "ccmcp/storage/inmemory_interaction_repository.h"
#include "ccmcp/storage/inmemory_opportunity_repository.h"
#include "ccmcp/vector/null_embedding_index.h"

#include <catch2/catch_test_macros.hpp>

using namespace ccmcp;

TEST_CASE("app_service: run_interaction_transition with valid event",
          "[app_service][interaction]") {
  // Arrange
  core::DeterministicIdGenerator id_gen;
  core::FixedClock clock("2026-01-01T00:00:00Z");

  storage::InMemoryAtomRepository atom_repo;
  storage::InMemoryOpportunityRepository opportunity_repo;
  storage::InMemoryInteractionRepository interaction_repo;
  storage::InMemoryAuditLog audit_log;
  vector::NullEmbeddingIndex vector_index;
  embedding::DeterministicStubEmbeddingProvider embedding_provider;

  core::Services services{atom_repo, opportunity_repo, interaction_repo,
                          audit_log, vector_index,     embedding_provider};

  interaction::InMemoryInteractionCoordinator coordinator;

  // Create interaction
  core::InteractionId int_id = core::new_interaction_id(id_gen);
  core::ContactId contact_id = core::new_contact_id(id_gen);
  core::OpportunityId opp_id = core::new_opportunity_id(id_gen);

  bool created = coordinator.create_interaction(int_id, contact_id, opp_id);
  REQUIRE(created);

  // Act: Apply kPrepare transition
  app::InteractionTransitionRequest request{
      .interaction_id = int_id,
      .event = domain::InteractionEvent::kPrepare,
      .idempotency_key = "test-idem-key-001",
  };

  auto response = app::run_interaction_transition(request, coordinator, services, id_gen, clock);

  // Assert: Transition applied
  CHECK(response.result.outcome == interaction::TransitionOutcome::kApplied);
  CHECK(response.result.before_state == domain::InteractionState::kDraft);
  CHECK(response.result.after_state == domain::InteractionState::kReady);
  CHECK(response.result.transition_index == 1);

  // Assert: Audit events logged
  auto events = services.audit_log.query(response.trace_id);
  REQUIRE(events.size() == 2);
  CHECK(events[0].event_type == "InteractionTransitionAttempted");
  CHECK(events[1].event_type == "InteractionTransitionCompleted");
}

TEST_CASE("app_service: run_interaction_transition with idempotency",
          "[app_service][interaction][idempotency]") {
  // Arrange
  core::DeterministicIdGenerator id_gen;
  core::FixedClock clock("2026-01-01T00:00:00Z");

  storage::InMemoryAtomRepository atom_repo;
  storage::InMemoryOpportunityRepository opportunity_repo;
  storage::InMemoryInteractionRepository interaction_repo;
  storage::InMemoryAuditLog audit_log;
  vector::NullEmbeddingIndex vector_index;
  embedding::DeterministicStubEmbeddingProvider embedding_provider;

  core::Services services{atom_repo, opportunity_repo, interaction_repo,
                          audit_log, vector_index,     embedding_provider};

  interaction::InMemoryInteractionCoordinator coordinator;

  core::InteractionId int_id = core::new_interaction_id(id_gen);
  coordinator.create_interaction(int_id, core::new_contact_id(id_gen),
                                 core::new_opportunity_id(id_gen));

  app::InteractionTransitionRequest request{
      .interaction_id = int_id,
      .event = domain::InteractionEvent::kPrepare,
      .idempotency_key = "idem-key-123",
  };

  // Act: Apply same transition twice with same idempotency key
  auto response1 = app::run_interaction_transition(request, coordinator, services, id_gen, clock);
  auto response2 = app::run_interaction_transition(request, coordinator, services, id_gen, clock);

  // Assert: First succeeds
  CHECK(response1.result.outcome == interaction::TransitionOutcome::kApplied);
  CHECK(response1.result.transition_index == 1);

  // Assert: Second returns AlreadyApplied
  CHECK(response2.result.outcome == interaction::TransitionOutcome::kAlreadyApplied);
  CHECK(response2.result.after_state == domain::InteractionState::kReady);
  CHECK(response2.result.transition_index == 1);  // Same index as first

  // Assert: Both share the same trace_id (if provided), or have different trace_ids
  // For this test, we're generating new trace_ids each time
  CHECK(!response1.trace_id.empty());
  CHECK(!response2.trace_id.empty());
}

TEST_CASE("app_service: run_interaction_transition with invalid event",
          "[app_service][interaction][error]") {
  // Arrange
  core::DeterministicIdGenerator id_gen;
  core::FixedClock clock("2026-01-01T00:00:00Z");

  storage::InMemoryAtomRepository atom_repo;
  storage::InMemoryOpportunityRepository opportunity_repo;
  storage::InMemoryInteractionRepository interaction_repo;
  storage::InMemoryAuditLog audit_log;
  vector::NullEmbeddingIndex vector_index;
  embedding::DeterministicStubEmbeddingProvider embedding_provider;

  core::Services services{atom_repo, opportunity_repo, interaction_repo,
                          audit_log, vector_index,     embedding_provider};

  interaction::InMemoryInteractionCoordinator coordinator;

  core::InteractionId int_id = core::new_interaction_id(id_gen);
  coordinator.create_interaction(int_id, core::new_contact_id(id_gen),
                                 core::new_opportunity_id(id_gen));

  // Act: Try invalid transition (kSend from kDraft - not allowed)
  app::InteractionTransitionRequest request{
      .interaction_id = int_id,
      .event = domain::InteractionEvent::kSend,
      .idempotency_key = "test-invalid",
  };

  auto response = app::run_interaction_transition(request, coordinator, services, id_gen, clock);

  // Assert: Transition rejected
  CHECK(response.result.outcome == interaction::TransitionOutcome::kInvalidTransition);
  CHECK(response.result.before_state == domain::InteractionState::kDraft);
  CHECK(response.result.after_state == domain::InteractionState::kDraft);
  CHECK(response.result.transition_index == 0);

  // Assert: Rejection event logged
  auto events = services.audit_log.query(response.trace_id);
  REQUIRE(events.size() == 2);
  CHECK(events[0].event_type == "InteractionTransitionAttempted");
  CHECK(events[1].event_type == "InteractionTransitionRejected");
}

TEST_CASE("app_service: fetch_audit_trace returns events for trace_id", "[app_service][audit]") {
  // Arrange
  core::DeterministicIdGenerator id_gen;
  core::FixedClock clock("2026-01-01T00:00:00Z");

  storage::InMemoryAtomRepository atom_repo;
  storage::InMemoryOpportunityRepository opportunity_repo;
  storage::InMemoryInteractionRepository interaction_repo;
  storage::InMemoryAuditLog audit_log;
  vector::NullEmbeddingIndex vector_index;
  embedding::DeterministicStubEmbeddingProvider embedding_provider;

  core::Services services{atom_repo, opportunity_repo, interaction_repo,
                          audit_log, vector_index,     embedding_provider};

  // Manually append some events with a specific trace_id
  const std::string trace_id = "test-trace-123";
  services.audit_log.append(
      {id_gen.next("evt"), trace_id, "TestEvent1", "{}", clock.now_iso8601(), {}});
  services.audit_log.append(
      {id_gen.next("evt"), trace_id, "TestEvent2", "{}", clock.now_iso8601(), {}});
  services.audit_log.append(
      {id_gen.next("evt"), "other-trace", "TestEvent3", "{}", clock.now_iso8601(), {}});

  // Act
  auto events = app::fetch_audit_trace(trace_id, services);

  // Assert: Only events with matching trace_id returned
  REQUIRE(events.size() == 2);
  CHECK(events[0].event_type == "TestEvent1");
  CHECK(events[1].event_type == "TestEvent2");
}
