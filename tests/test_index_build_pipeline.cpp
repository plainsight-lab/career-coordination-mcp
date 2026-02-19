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

// Open an in-memory SQLite DB with schema v6 (chained: v1→v6).
static std::shared_ptr<storage::sqlite::SqliteDb> make_db() {
  auto result = storage::sqlite::SqliteDb::open(":memory:");
  REQUIRE(result.has_value());
  auto db = result.value();
  REQUIRE(db->ensure_schema_v6().has_value());
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

// ---------------------------------------------------------------------------
// v0.4 Slice 1 — Drift detection and determinism tests
// ---------------------------------------------------------------------------

// Test B: Drift detection works when run_id increments across simulated invocations.
// A new SqliteIndexRunStore on the same DB simulates a fresh CLI invocation that
// previously would have reset DeterministicIdGenerator and always produced run-0.
TEST_CASE("index-build drift detection works across separate run_store instances",
          "[indexing][pipeline]") {
  auto db = make_db();
  storage::InMemoryAtomRepository atom_repo;
  InMemoryResumeStore resume_store;
  storage::InMemoryOpportunityRepository opp_repo;
  vector::InMemoryEmbeddingIndex vector_index;
  embedding::DeterministicStubEmbeddingProvider embedding_provider(128);
  storage::InMemoryAuditLog audit_log;
  core::FixedClock clock("2026-01-01T00:00:00Z");

  atom_repo.upsert({core::AtomId{"atom-001"}, "cpp", "C++", "Claim", {"cpp20"}, true, {}});

  auto config = default_config("atoms");

  // First "invocation": fresh generator, fresh store — simulates first CLI run.
  {
    storage::sqlite::SqliteIndexRunStore run_store(db);
    core::DeterministicIdGenerator id_gen;
    auto first =
        indexing::run_index_build(atom_repo, resume_store, opp_repo, run_store, vector_index,
                                  embedding_provider, audit_log, id_gen, clock, config);
    CHECK(first.indexed_count == 1);
    CHECK(first.skipped_count == 0);
    CHECK(first.run_id == "run-1");
  }

  // Second "invocation": fresh generator, fresh store on same DB — simulates second CLI run.
  // Without the fix this would always produce run-0, overwrite the prior completed run,
  // and re-index everything. With the fix, run-2 is allocated and drift detection works.
  {
    storage::sqlite::SqliteIndexRunStore run_store(db);
    core::DeterministicIdGenerator id_gen;
    auto second =
        indexing::run_index_build(atom_repo, resume_store, opp_repo, run_store, vector_index,
                                  embedding_provider, audit_log, id_gen, clock, config);
    CHECK(second.indexed_count == 0);  // unchanged source — correctly skipped
    CHECK(second.skipped_count == 1);
    CHECK(second.stale_count == 0);
    CHECK(second.run_id == "run-2");  // unique ID — does not overwrite run-1
  }
}

// Test C: Artifact source_hash and vector_hash are deterministic — identical canonical
// text produces identical hashes regardless of which run or database recorded them.
// Uses two independent in-memory databases to verify the invariant without depending
// on drift detection ordering between same-timestamp runs.
TEST_CASE("index-build artifact hashes are deterministic across independent databases",
          "[indexing][pipeline]") {
  // Helper: run a single index-build on a fresh in-memory database and return the entries.
  auto run_pipeline = [](const std::string& claim) {
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

    atom_repo.upsert({core::AtomId{"atom-001"}, "cpp", "C++", claim, {}, true, {}});
    auto result = indexing::run_index_build(atom_repo, resume_store, opp_repo, run_store,
                                            vector_index, embedding_provider, audit_log, id_gen,
                                            clock, default_config("atoms"));
    return run_store.get_entries_for_run(result.run_id);
  };

  // Run once with content A, once with content B, once with content A again.
  // The first and third runs must produce byte-identical hashes for the same text.
  const auto entries_a1 = run_pipeline("Deterministic claim for testing");
  const auto entries_b = run_pipeline("Different claim");
  const auto entries_a2 = run_pipeline("Deterministic claim for testing");

  REQUIRE(entries_a1.size() == 1);
  REQUIRE(entries_b.size() == 1);
  REQUIRE(entries_a2.size() == 1);

  // Same canonical text → same hashes (functional determinism).
  CHECK(entries_a1[0].source_hash == entries_a2[0].source_hash);
  CHECK(entries_a1[0].vector_hash == entries_a2[0].vector_hash);

  // Different canonical text → different source_hash (collision resistance sanity check).
  CHECK(entries_a1[0].source_hash != entries_b[0].source_hash);
}
