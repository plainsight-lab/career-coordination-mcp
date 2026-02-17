#include "ccmcp/app/app_service.h"
#include "ccmcp/constitution/validation_report.h"
#include "ccmcp/core/clock.h"
#include "ccmcp/core/id_generator.h"
#include "ccmcp/domain/opportunity.h"
#include "ccmcp/domain/requirement.h"
#include "ccmcp/embedding/embedding_provider.h"
#include "ccmcp/storage/audit_log.h"
#include "ccmcp/storage/inmemory_atom_repository.h"
#include "ccmcp/storage/inmemory_interaction_repository.h"
#include "ccmcp/storage/inmemory_opportunity_repository.h"
#include "ccmcp/vector/null_embedding_index.h"

#include <catch2/catch_test_macros.hpp>

using namespace ccmcp;

TEST_CASE("app_service: run_match_pipeline with deterministic components",
          "[app_service][match][determinism]") {
  // Arrange: Create deterministic services
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

  // Create and store opportunity
  domain::Opportunity opportunity{};
  opportunity.opportunity_id = core::new_opportunity_id(id_gen);
  opportunity.company = "ExampleCo";
  opportunity.role_title = "Principal Architect";
  opportunity.source = "test";
  opportunity.requirements = {
      domain::Requirement{"C++20", {"cpp", "cpp20"}, true},
      domain::Requirement{"Architecture experience", {"architecture"}, true},
  };
  services.opportunities.upsert(opportunity);

  // Create and store atoms
  services.atoms.upsert({core::new_atom_id(id_gen),
                         "architecture",
                         "Architecture Leadership",
                         "Led architecture decisions",
                         {"architecture", "governance"},
                         true,
                         {}});
  services.atoms.upsert({core::new_atom_id(id_gen),
                         "cpp",
                         "Modern C++",
                         "Built C++20 systems",
                         {"cpp20", "systems"},
                         true,
                         {}});

  // Act: Run match pipeline
  app::MatchPipelineRequest request{
      .opportunity_id = opportunity.opportunity_id,
      // Default: use all verified atoms
      .strategy = matching::MatchingStrategy::kDeterministicLexicalV01,
  };

  auto response = app::run_match_pipeline(request, services, id_gen, clock);

  // Assert: Match report produced
  CHECK(response.match_report.opportunity_id == opportunity.opportunity_id);
  CHECK(response.match_report.overall_score >= 0.0);
  CHECK(response.match_report.matched_atoms.size() >= 0);

  // Assert: Validation report produced
  CHECK((response.validation_report.status == constitution::ValidationStatus::kAccepted ||
         response.validation_report.status == constitution::ValidationStatus::kRejected ||
         response.validation_report.status == constitution::ValidationStatus::kBlocked));

  // Assert: Audit events logged
  auto events = services.audit_log.query(response.trace_id);
  REQUIRE(events.size() >= 4);

  // Events should be: RunStarted, MatchCompleted, ValidationCompleted, RunCompleted
  CHECK(events[0].event_type == "RunStarted");
  CHECK(events[1].event_type == "MatchCompleted");
  CHECK(events[2].event_type == "ValidationCompleted");
  CHECK(events[3].event_type == "RunCompleted");

  // All events should have same trace_id
  for (const auto& event : events) {
    CHECK(event.trace_id == response.trace_id);
  }
}

TEST_CASE("app_service: run_match_pipeline determinism (same input = same output)",
          "[app_service][match][determinism]") {
  // Run pipeline twice with same inputs, expect identical results

  auto run_once = []() {
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

    domain::Opportunity opportunity{};
    opportunity.opportunity_id = core::new_opportunity_id(id_gen);
    opportunity.company = "TestCo";
    opportunity.role_title = "Test Role";
    opportunity.source = "test";
    opportunity.requirements = {domain::Requirement{"Test skill", {"test"}, true}};

    services.atoms.upsert(
        {core::new_atom_id(id_gen), "test", "Test Skill", "Test description", {"test"}, true, {}});

    app::MatchPipelineRequest request{
        .opportunity = opportunity,
        .strategy = matching::MatchingStrategy::kDeterministicLexicalV01,
    };

    return app::run_match_pipeline(request, services, id_gen, clock);
  };

  auto response1 = run_once();
  auto response2 = run_once();

  // Same overall score (deterministic matching)
  CHECK(response1.match_report.overall_score == response2.match_report.overall_score);

  // Same validation status (deterministic validation)
  CHECK(response1.validation_report.status == response2.validation_report.status);
  CHECK(response1.validation_report.findings.size() == response2.validation_report.findings.size());
}

TEST_CASE("app_service: run_match_pipeline throws on missing opportunity",
          "[app_service][match][error]") {
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

  app::MatchPipelineRequest request{
      .opportunity_id = core::OpportunityId{"nonexistent"},
  };

  CHECK_THROWS_AS(app::run_match_pipeline(request, services, id_gen, clock), std::invalid_argument);
}

TEST_CASE(
    "app_service: run_match_pipeline throws when neither opportunity nor opportunity_id provided",
    "[app_service][match][error]") {
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

  app::MatchPipelineRequest request{
      // No opportunity or opportunity_id
  };

  CHECK_THROWS_AS(app::run_match_pipeline(request, services, id_gen, clock), std::invalid_argument);
}
