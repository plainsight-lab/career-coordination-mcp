#include "ccmcp/app/app_service.h"
#include "ccmcp/core/clock.h"
#include "ccmcp/core/id_generator.h"
#include "ccmcp/embedding/embedding_provider.h"
#include "ccmcp/ingest/resume_ingestor.h"
#include "ccmcp/storage/audit_log.h"
#include "ccmcp/storage/inmemory_atom_repository.h"
#include "ccmcp/storage/inmemory_interaction_repository.h"
#include "ccmcp/storage/inmemory_opportunity_repository.h"
#include "ccmcp/storage/sqlite/sqlite_db.h"
#include "ccmcp/storage/sqlite/sqlite_resume_store.h"
#include "ccmcp/vector/null_embedding_index.h"

#include <catch2/catch_test_macros.hpp>

#include <fstream>
#include <string>

using namespace ccmcp;

namespace {

// Minimal fixture for the ingest pipeline: in-memory SQLite for IResumeStore,
// plus in-memory everything else (no disk I/O required).
struct IngestPipelineFixture {
  core::DeterministicIdGenerator id_gen;
  core::FixedClock clock{"2026-01-01T00:00:00Z"};

  storage::InMemoryAtomRepository atom_repo;
  storage::InMemoryOpportunityRepository opportunity_repo;
  storage::InMemoryInteractionRepository interaction_repo;
  storage::InMemoryAuditLog audit_log;
  vector::NullEmbeddingIndex vector_index;
  embedding::DeterministicStubEmbeddingProvider embedding_provider;
  core::Services services{atom_repo, opportunity_repo, interaction_repo,
                          audit_log, vector_index,     embedding_provider};

  std::shared_ptr<storage::sqlite::SqliteDb> db;
  storage::sqlite::SqliteResumeStore resume_store;
  std::unique_ptr<ingest::IResumeIngestor> ingestor_owner;
  ingest::IResumeIngestor& ingestor;

  IngestPipelineFixture()
      : db([] {
          auto r = storage::sqlite::SqliteDb::open(":memory:");
          REQUIRE(r.has_value());
          auto s = r.value()->ensure_schema_v4();
          REQUIRE(s.has_value());
          return r.value();
        }()),
        resume_store(db),
        ingestor_owner(ingest::create_resume_ingestor()),
        ingestor(*ingestor_owner) {}

  // Write a temp markdown file and return its path. Caller must remove it.
  static std::string make_temp_md(const std::string& name, const std::string& content) {
    std::ofstream ofs(name);
    ofs << content;
    return name;
  }
};

}  // namespace

TEST_CASE("run_ingest_resume_pipeline: returns non-empty response fields",
          "[app_service][ingest_pipeline]") {
  IngestPipelineFixture fx;
  const std::string path = fx.make_temp_md("tmp_ingest_basic.md", "# Jane Doe\n## Skills\n- C++\n");

  app::IngestResumePipelineRequest req;
  req.input_path = path;
  req.persist = false;

  auto response = app::run_ingest_resume_pipeline(req, fx.ingestor, fx.resume_store, fx.services,
                                                  fx.id_gen, fx.clock);
  std::remove(path.c_str());

  CHECK(!response.resume_id.empty());
  CHECK(!response.resume_hash.empty());
  CHECK(!response.source_hash.empty());
  CHECK(!response.trace_id.empty());
}

TEST_CASE("run_ingest_resume_pipeline: persist=true stores resume in store",
          "[app_service][ingest_pipeline]") {
  IngestPipelineFixture fx;
  const std::string path =
      fx.make_temp_md("tmp_ingest_persist.md", "# Bob Builder\n## Work\n- Construction\n");

  app::IngestResumePipelineRequest req;
  req.input_path = path;
  req.persist = true;

  auto response = app::run_ingest_resume_pipeline(req, fx.ingestor, fx.resume_store, fx.services,
                                                  fx.id_gen, fx.clock);
  std::remove(path.c_str());

  core::ResumeId rid{response.resume_id};
  auto stored = fx.resume_store.get(rid);
  REQUIRE(stored.has_value());
  CHECK(stored.value().resume_id.value == response.resume_id);
}

TEST_CASE("run_ingest_resume_pipeline: persist=false does not store resume",
          "[app_service][ingest_pipeline]") {
  IngestPipelineFixture fx;
  const std::string path =
      fx.make_temp_md("tmp_ingest_no_persist.md", "# Alice Smith\n## Skills\n- Rust\n");

  app::IngestResumePipelineRequest req;
  req.input_path = path;
  req.persist = false;

  auto response = app::run_ingest_resume_pipeline(req, fx.ingestor, fx.resume_store, fx.services,
                                                  fx.id_gen, fx.clock);
  std::remove(path.c_str());

  core::ResumeId rid{response.resume_id};
  auto stored = fx.resume_store.get(rid);
  CHECK(!stored.has_value());
}

TEST_CASE("run_ingest_resume_pipeline: provided trace_id is preserved",
          "[app_service][ingest_pipeline]") {
  IngestPipelineFixture fx;
  const std::string path =
      fx.make_temp_md("tmp_ingest_trace.md", "# Carol White\n## Experience\n- PM\n");

  app::IngestResumePipelineRequest req;
  req.input_path = path;
  req.persist = false;
  req.trace_id = "trace-my-custom-id-001";

  auto response = app::run_ingest_resume_pipeline(req, fx.ingestor, fx.resume_store, fx.services,
                                                  fx.id_gen, fx.clock);
  std::remove(path.c_str());

  CHECK(response.trace_id == "trace-my-custom-id-001");
}

TEST_CASE("run_ingest_resume_pipeline: emits IngestStarted and IngestCompleted audit events",
          "[app_service][ingest_pipeline]") {
  IngestPipelineFixture fx;
  const std::string path = fx.make_temp_md("tmp_ingest_audit.md", "# David Grey\n## Work\n- SWE\n");

  app::IngestResumePipelineRequest req;
  req.input_path = path;
  req.persist = false;
  req.trace_id = "trace-audit-check";

  auto audit_resp = app::run_ingest_resume_pipeline(req, fx.ingestor, fx.resume_store, fx.services,
                                                    fx.id_gen, fx.clock);
  (void)audit_resp;
  std::remove(path.c_str());

  auto events = fx.audit_log.query("trace-audit-check");
  REQUIRE(events.size() >= 2);
  CHECK(events[0].event_type == "IngestStarted");
  CHECK(events[1].event_type == "IngestCompleted");
  for (const auto& evt : events) {
    CHECK(evt.trace_id == "trace-audit-check");
  }
}

TEST_CASE("run_ingest_resume_pipeline: nonexistent file throws runtime_error",
          "[app_service][ingest_pipeline][error]") {
  IngestPipelineFixture fx;

  app::IngestResumePipelineRequest req;
  req.input_path = "/nonexistent/path/that/does/not/exist.md";
  req.persist = false;

  auto do_ingest = [&]() {
    auto r = app::run_ingest_resume_pipeline(req, fx.ingestor, fx.resume_store, fx.services,
                                             fx.id_gen, fx.clock);
    (void)r;
  };
  CHECK_THROWS_AS(do_ingest(), std::runtime_error);
}
