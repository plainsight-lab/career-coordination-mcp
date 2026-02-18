#include "ccmcp/core/clock.h"
#include "ccmcp/core/id_generator.h"
#include "ccmcp/core/ids.h"
#include "ccmcp/domain/experience_atom.h"
#include "ccmcp/domain/opportunity.h"
#include "ccmcp/domain/requirement.h"
#include "ccmcp/embedding/embedding_provider.h"
#include "ccmcp/indexing/index_build_pipeline.h"
#include "ccmcp/ingest/ingested_resume.h"
#include "ccmcp/storage/audit_log.h"
#include "ccmcp/storage/inmemory_atom_repository.h"
#include "ccmcp/storage/inmemory_opportunity_repository.h"
#include "ccmcp/storage/sqlite/sqlite_db.h"
#include "ccmcp/storage/sqlite/sqlite_index_run_store.h"
#include "ccmcp/vector/inmemory_embedding_index.h"

#include <catch2/catch_test_macros.hpp>

using namespace ccmcp;

// ---------------------------------------------------------------------------
// Test fixtures and helpers
// ---------------------------------------------------------------------------

// InMemoryResumeStore: simple IResumeStore for testing.
class InMemoryResumeStore final : public ingest::IResumeStore {
 public:
  void upsert(const ingest::IngestedResume& resume) override {
    resumes_[resume.resume_id.value] = resume;
  }

  std::optional<ingest::IngestedResume> get(const core::ResumeId& id) const override {
    auto it = resumes_.find(id.value);
    if (it == resumes_.end())
      return std::nullopt;
    return it->second;
  }

  std::optional<ingest::IngestedResume> get_by_hash(const std::string& hash) const override {
    for (const auto& [id, r] : resumes_) {
      if (r.resume_hash == hash)
        return r;
    }
    return std::nullopt;
  }

  std::vector<ingest::IngestedResume> list_all() const override {
    std::vector<ingest::IngestedResume> result;
    result.reserve(resumes_.size());
    for (const auto& [id, r] : resumes_) {
      result.push_back(r);
    }
    return result;
  }

 private:
  std::map<std::string, ingest::IngestedResume> resumes_;
};

// Open an in-memory SQLite DB with schema v4.
static std::shared_ptr<storage::sqlite::SqliteDb> make_db() {
  auto result = storage::sqlite::SqliteDb::open(":memory:");
  REQUIRE(result.has_value());
  auto db = result.value();
  REQUIRE(db->ensure_schema_v4().has_value());
  return db;
}

// Build default config (scope=atoms, deterministic-stub provider).
static indexing::IndexBuildConfig default_config(const std::string& scope = "atoms") {
  return {scope, "deterministic-stub", "", ""};
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST_CASE("index-build indexes expected atoms", "[indexing][pipeline]") {
  auto db = make_db();
  storage::sqlite::SqliteIndexRunStore run_store(db);
  storage::InMemoryAtomRepository atom_repo;
  InMemoryResumeStore resume_store;
  storage::InMemoryOpportunityRepository opp_repo;
  vector::InMemoryEmbeddingIndex vector_index;
  embedding::DeterministicStubEmbeddingProvider embedding_provider(128);
  storage::InMemoryAuditLog audit_log;
  core::DeterministicIdGenerator id_gen;
  core::FixedClock clock("2026-01-01T00:00:00Z");

  atom_repo.upsert({core::AtomId{"atom-001"},
                    "cpp",
                    "Modern C++",
                    "Built C++20 systems",
                    {"cpp20", "systems"},
                    true,
                    {}});
  atom_repo.upsert({core::AtomId{"atom-002"},
                    "arch",
                    "Architecture",
                    "Led arch decisions",
                    {"architecture"},
                    true,
                    {}});

  auto result = indexing::run_index_build(atom_repo, resume_store, opp_repo, run_store,
                                          vector_index, embedding_provider, audit_log, id_gen,
                                          clock, default_config("atoms"));

  CHECK(result.indexed_count == 2);
  CHECK(result.skipped_count == 0);
  CHECK(result.stale_count == 0);
  CHECK(!result.run_id.empty());

  // Verify index entries were written.
  auto entries = run_store.get_entries_for_run(result.run_id);
  CHECK(entries.size() == 2);

  // Verify vectors were stored.
  auto vec1 = vector_index.get("atom-001");
  CHECK(vec1.has_value());
  CHECK(vec1->size() == 128);

  auto vec2 = vector_index.get("atom-002");
  CHECK(vec2.has_value());
}

TEST_CASE("index-build skips resumes when scope=atoms", "[indexing][pipeline]") {
  auto db = make_db();
  storage::sqlite::SqliteIndexRunStore run_store(db);
  storage::InMemoryAtomRepository atom_repo;
  InMemoryResumeStore resume_store;
  storage::InMemoryOpportunityRepository opp_repo;
  vector::InMemoryEmbeddingIndex vector_index;
  embedding::DeterministicStubEmbeddingProvider embedding_provider(128);
  storage::InMemoryAuditLog audit_log;
  core::DeterministicIdGenerator id_gen;
  core::FixedClock clock("2026-01-01T00:00:00Z");

  // Insert one atom and one resume.
  atom_repo.upsert({core::AtomId{"atom-001"}, "cpp", "C++", "Claim", {}, true, {}});
  resume_store.upsert(
      {core::ResumeId{"resume-001"}, "# CV\nSome content.", "hash-r1", {}, std::nullopt});

  auto result = indexing::run_index_build(atom_repo, resume_store, opp_repo, run_store,
                                          vector_index, embedding_provider, audit_log, id_gen,
                                          clock, default_config("atoms"));

  CHECK(result.indexed_count == 1);
  // Resume vector key should not be present.
  CHECK(!vector_index.get("resume:resume-001").has_value());
}

TEST_CASE("index-build is idempotent on rerun with same source", "[indexing][pipeline]") {
  auto db = make_db();
  storage::sqlite::SqliteIndexRunStore run_store(db);
  storage::InMemoryAtomRepository atom_repo;
  InMemoryResumeStore resume_store;
  storage::InMemoryOpportunityRepository opp_repo;
  vector::InMemoryEmbeddingIndex vector_index;
  embedding::DeterministicStubEmbeddingProvider embedding_provider(128);
  storage::InMemoryAuditLog audit_log;
  core::DeterministicIdGenerator id_gen;
  core::FixedClock clock("2026-01-01T00:00:00Z");

  atom_repo.upsert({core::AtomId{"atom-001"}, "cpp", "C++", "Claim", {"cpp20"}, true, {}});
  atom_repo.upsert({core::AtomId{"atom-002"}, "arch", "Arch", "Claim2", {}, true, {}});

  auto config = default_config("atoms");

  // First run — both atoms should be indexed.
  auto first = indexing::run_index_build(atom_repo, resume_store, opp_repo, run_store, vector_index,
                                         embedding_provider, audit_log, id_gen, clock, config);
  CHECK(first.indexed_count == 2);
  CHECK(first.skipped_count == 0);

  // Second run with identical source — both atoms should be skipped.
  auto second =
      indexing::run_index_build(atom_repo, resume_store, opp_repo, run_store, vector_index,
                                embedding_provider, audit_log, id_gen, clock, config);
  CHECK(second.indexed_count == 0);
  CHECK(second.skipped_count == 2);
  CHECK(second.stale_count == 0);
}

TEST_CASE("index-build detects stale when atom changes", "[indexing][pipeline]") {
  auto db = make_db();
  storage::sqlite::SqliteIndexRunStore run_store(db);
  storage::InMemoryAtomRepository atom_repo;
  InMemoryResumeStore resume_store;
  storage::InMemoryOpportunityRepository opp_repo;
  vector::InMemoryEmbeddingIndex vector_index;
  embedding::DeterministicStubEmbeddingProvider embedding_provider(128);
  storage::InMemoryAuditLog audit_log;
  core::DeterministicIdGenerator id_gen;
  core::FixedClock clock("2026-01-01T00:00:00Z");

  atom_repo.upsert({core::AtomId{"atom-001"}, "cpp", "C++", "Original claim", {}, true, {}});
  atom_repo.upsert({core::AtomId{"atom-002"}, "arch", "Arch", "Stable claim", {}, true, {}});

  auto config = default_config("atoms");

  // First run — index both.
  auto first = indexing::run_index_build(atom_repo, resume_store, opp_repo, run_store, vector_index,
                                         embedding_provider, audit_log, id_gen, clock, config);
  REQUIRE(first.indexed_count == 2);

  // Modify atom-001.
  atom_repo.upsert({core::AtomId{"atom-001"}, "cpp", "C++", "Updated claim", {}, true, {}});

  // Second run — atom-001 is stale, atom-002 is unchanged.
  auto second =
      indexing::run_index_build(atom_repo, resume_store, opp_repo, run_store, vector_index,
                                embedding_provider, audit_log, id_gen, clock, config);
  CHECK(second.indexed_count == 1);  // atom-001 re-indexed
  CHECK(second.skipped_count == 1);  // atom-002 skipped
  CHECK(second.stale_count == 1);    // atom-001 was stale
}

TEST_CASE("index-build with NullEmbeddingProvider skips all", "[indexing][pipeline]") {
  auto db = make_db();
  storage::sqlite::SqliteIndexRunStore run_store(db);
  storage::InMemoryAtomRepository atom_repo;
  InMemoryResumeStore resume_store;
  storage::InMemoryOpportunityRepository opp_repo;
  vector::InMemoryEmbeddingIndex vector_index;
  embedding::NullEmbeddingProvider null_provider;
  storage::InMemoryAuditLog audit_log;
  core::DeterministicIdGenerator id_gen;
  core::FixedClock clock("2026-01-01T00:00:00Z");

  atom_repo.upsert({core::AtomId{"atom-001"}, "cpp", "C++", "Claim", {}, true, {}});
  atom_repo.upsert({core::AtomId{"atom-002"}, "arch", "Arch", "Claim2", {}, true, {}});

  auto result =
      indexing::run_index_build(atom_repo, resume_store, opp_repo, run_store, vector_index,
                                null_provider, audit_log, id_gen, clock, default_config("atoms"));

  // NullEmbeddingProvider returns empty vectors — nothing should be indexed.
  CHECK(result.indexed_count == 0);
  CHECK(result.skipped_count == 0);

  // No entries written.
  auto entries = run_store.get_entries_for_run(result.run_id);
  CHECK(entries.empty());

  // Run should still be completed.
  auto run = run_store.get_run(result.run_id);
  REQUIRE(run.has_value());
  CHECK(run->status == indexing::IndexRunStatus::kCompleted);
}

TEST_CASE("index-build emits audit events", "[indexing][pipeline]") {
  auto db = make_db();
  storage::sqlite::SqliteIndexRunStore run_store(db);
  storage::InMemoryAtomRepository atom_repo;
  InMemoryResumeStore resume_store;
  storage::InMemoryOpportunityRepository opp_repo;
  vector::InMemoryEmbeddingIndex vector_index;
  embedding::DeterministicStubEmbeddingProvider embedding_provider(128);
  storage::InMemoryAuditLog audit_log;
  core::DeterministicIdGenerator id_gen;
  core::FixedClock clock("2026-01-01T00:00:00Z");

  atom_repo.upsert({core::AtomId{"atom-001"}, "cpp", "C++", "Claim", {}, true, {}});

  auto result = indexing::run_index_build(atom_repo, resume_store, opp_repo, run_store,
                                          vector_index, embedding_provider, audit_log, id_gen,
                                          clock, default_config("atoms"));

  // Audit events use run_id as trace_id.
  auto events = audit_log.query(result.run_id);
  REQUIRE(events.size() >= 3);

  // First event must be IndexRunStarted.
  CHECK(events[0].event_type == "IndexRunStarted");

  // Middle event(s) are IndexedArtifact.
  bool found_indexed = false;
  for (const auto& evt : events) {
    if (evt.event_type == "IndexedArtifact") {
      found_indexed = true;
    }
  }
  CHECK(found_indexed);

  // Last event must be IndexRunCompleted.
  CHECK(events.back().event_type == "IndexRunCompleted");
}

TEST_CASE("index-build scope=all indexes atoms+resumes+opps", "[indexing][pipeline]") {
  auto db = make_db();
  storage::sqlite::SqliteIndexRunStore run_store(db);
  storage::InMemoryAtomRepository atom_repo;
  InMemoryResumeStore resume_store;
  storage::InMemoryOpportunityRepository opp_repo;
  vector::InMemoryEmbeddingIndex vector_index;
  embedding::DeterministicStubEmbeddingProvider embedding_provider(128);
  storage::InMemoryAuditLog audit_log;
  core::DeterministicIdGenerator id_gen;
  core::FixedClock clock("2026-01-01T00:00:00Z");

  atom_repo.upsert({core::AtomId{"atom-001"}, "cpp", "C++", "Claim", {}, true, {}});

  domain::Requirement req{"5+ years C++", {}, true};
  domain::Opportunity opp{
      core::OpportunityId{"opp-001"}, "ExampleCo", "Principal Architect", {req}, "manual"};
  opp_repo.upsert(opp);

  resume_store.upsert(
      {core::ResumeId{"resume-001"}, "# CV\nContent.", "hash-r1", {}, std::nullopt});

  auto result = indexing::run_index_build(atom_repo, resume_store, opp_repo, run_store,
                                          vector_index, embedding_provider, audit_log, id_gen,
                                          clock, default_config("all"));

  CHECK(result.indexed_count == 3);
  CHECK(result.skipped_count == 0);

  // Check each vector type was stored.
  CHECK(vector_index.get("atom-001").has_value());
  CHECK(vector_index.get("resume:resume-001").has_value());
  CHECK(vector_index.get("opp:opp-001").has_value());
}
