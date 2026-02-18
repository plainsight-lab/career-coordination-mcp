#include "ccmcp/app/app_service.h"
#include "ccmcp/core/clock.h"
#include "ccmcp/core/id_generator.h"
#include "ccmcp/core/ids.h"
#include "ccmcp/domain/opportunity.h"
#include "ccmcp/domain/requirement.h"
#include "ccmcp/embedding/embedding_provider.h"
#include "ccmcp/storage/audit_log.h"
#include "ccmcp/storage/inmemory_atom_repository.h"
#include "ccmcp/storage/inmemory_interaction_repository.h"
#include "ccmcp/storage/inmemory_opportunity_repository.h"
#include "ccmcp/storage/sqlite/sqlite_db.h"
#include "ccmcp/storage/sqlite/sqlite_index_run_store.h"
#include "ccmcp/storage/sqlite/sqlite_resume_store.h"
#include "ccmcp/vector/inmemory_embedding_index.h"

#include <catch2/catch_test_macros.hpp>

#include <string>

using namespace ccmcp;

namespace {

// Fixture for the index-build pipeline.
// Uses in-memory SQLite for IResumeStore + IIndexRunStore (the only implementations),
// InMemoryEmbeddingIndex for the vector index, and DeterministicStubEmbeddingProvider.
struct IndexBuildPipelineFixture {
  core::DeterministicIdGenerator id_gen;
  core::FixedClock clock{"2026-01-01T00:00:00Z"};

  storage::InMemoryAtomRepository atom_repo;
  storage::InMemoryOpportunityRepository opportunity_repo;
  storage::InMemoryInteractionRepository interaction_repo;
  storage::InMemoryAuditLog audit_log;
  vector::InMemoryEmbeddingIndex vector_index;
  embedding::DeterministicStubEmbeddingProvider embedding_provider;
  core::Services services{atom_repo, opportunity_repo, interaction_repo,
                          audit_log, vector_index,     embedding_provider};

  std::shared_ptr<storage::sqlite::SqliteDb> db;
  storage::sqlite::SqliteResumeStore resume_store;
  storage::sqlite::SqliteIndexRunStore index_run_store;

  IndexBuildPipelineFixture()
      : db([] {
          auto r = storage::sqlite::SqliteDb::open(":memory:");
          REQUIRE(r.has_value());
          auto s = r.value()->ensure_schema_v4();
          REQUIRE(s.has_value());
          return r.value();
        }()),
        resume_store(db),
        index_run_store(db) {}

  app::IndexBuildPipelineResponse run(const std::string& scope = "all",
                                      std::optional<std::string> trace_id = std::nullopt) {
    app::IndexBuildPipelineRequest req;
    req.scope = scope;
    req.trace_id = trace_id;
    return app::run_index_build_pipeline(req, resume_store, index_run_store, services,
                                         "deterministic-stub", id_gen, clock);
  }
};

}  // namespace

TEST_CASE("run_index_build_pipeline: empty data sources return zero counts",
          "[app_service][index_build_pipeline]") {
  IndexBuildPipelineFixture fx;

  auto response = fx.run("all");

  CHECK(!response.run_id.empty());
  CHECK(!response.trace_id.empty());
  // With DeterministicStubEmbeddingProvider and no artifacts, indexed=0.
  CHECK(response.indexed_count == 0);
  CHECK(response.skipped_count == 0);
  CHECK(response.stale_count == 0);
}

TEST_CASE("run_index_build_pipeline: indexes atoms when present",
          "[app_service][index_build_pipeline]") {
  IndexBuildPipelineFixture fx;

  // Insert two atoms
  fx.services.atoms.upsert({core::new_atom_id(fx.id_gen),
                            "cpp",
                            "Modern C++",
                            "Built C++20 systems",
                            {"cpp20", "systems"},
                            true,
                            {}});
  fx.services.atoms.upsert({core::new_atom_id(fx.id_gen),
                            "arch",
                            "Systems Architecture",
                            "Designed distributed systems",
                            {"architecture"},
                            true,
                            {}});

  auto response = fx.run("atoms");

  // Both atoms should be indexed (DeterministicStub returns non-empty 128-dim vectors)
  CHECK(response.indexed_count == 2);
  CHECK(response.skipped_count == 0);
}

TEST_CASE("run_index_build_pipeline: provided trace_id is preserved",
          "[app_service][index_build_pipeline]") {
  IndexBuildPipelineFixture fx;

  auto response = fx.run("all", "trace-index-build-001");

  CHECK(response.trace_id == "trace-index-build-001");
}

TEST_CASE("run_index_build_pipeline: emits IndexBuildStarted and IndexBuildCompleted events",
          "[app_service][index_build_pipeline]") {
  IndexBuildPipelineFixture fx;

  auto response = fx.run("all", "trace-index-audit");

  auto events = fx.audit_log.query("trace-index-audit");
  // The pipeline wrapper emits IndexBuildStarted + IndexBuildCompleted.
  // Inner run_index_build uses a separate trace for its own events.
  REQUIRE(events.size() >= 2);
  CHECK(events[0].event_type == "IndexBuildStarted");
  CHECK(events[events.size() - 1].event_type == "IndexBuildCompleted");
  for (const auto& evt : events) {
    CHECK(evt.trace_id == "trace-index-audit");
  }
}

TEST_CASE("run_index_build_pipeline: second run with same atoms yields skipped=indexed, indexed=0",
          "[app_service][index_build_pipeline]") {
  IndexBuildPipelineFixture fx;

  fx.services.atoms.upsert({core::new_atom_id(fx.id_gen),
                            "go",
                            "Go Programming",
                            "Wrote production Go services",
                            {"golang"},
                            true,
                            {}});

  // First run — atom is new, should be indexed
  auto first = fx.run("atoms");
  REQUIRE(first.indexed_count == 1);
  REQUIRE(first.skipped_count == 0);

  // Second run — same atom, same source hash → skipped
  auto second = fx.run("atoms");
  CHECK(second.indexed_count == 0);
  CHECK(second.skipped_count == 1);
  CHECK(second.stale_count == 0);
}

TEST_CASE("run_index_build_pipeline: scope=atoms does not index opportunities",
          "[app_service][index_build_pipeline]") {
  IndexBuildPipelineFixture fx;

  // Add an opportunity (should not be indexed when scope=atoms)
  domain::Opportunity opp;
  opp.opportunity_id = core::new_opportunity_id(fx.id_gen);
  opp.company = "TestCo";
  opp.role_title = "Engineer";
  opp.source = "test";
  opp.requirements = {domain::Requirement{"Go", {"golang"}, true}};
  fx.services.opportunities.upsert(opp);

  // Add an atom too
  fx.services.atoms.upsert(
      {core::new_atom_id(fx.id_gen), "go", "Go Programming", "Go services", {"golang"}, true, {}});

  auto response = fx.run("atoms");

  // Only 1 atom should be indexed, not the opportunity
  CHECK(response.indexed_count == 1);
}
