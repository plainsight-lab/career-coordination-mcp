#include "ccmcp/domain/decision_record.h"
#include "ccmcp/storage/sqlite/sqlite_db.h"
#include "ccmcp/storage/sqlite/sqlite_decision_store.h"

#include <catch2/catch_test_macros.hpp>

using namespace ccmcp;

// Helper: open an in-memory DB with schema v5 applied.
static std::shared_ptr<storage::sqlite::SqliteDb> make_db() {
  auto result = storage::sqlite::SqliteDb::open(":memory:");
  REQUIRE(result.has_value());
  auto db = result.value();
  auto schema = db->ensure_schema_v5();
  REQUIRE(schema.has_value());
  return db;
}

// Helper: construct a minimal DecisionRecord.
static domain::DecisionRecord make_record(const std::string& decision_id,
                                          const std::string& trace_id,
                                          const std::string& opportunity_id = "opp-001") {
  domain::DecisionRecord r;
  r.decision_id = decision_id;
  r.trace_id = trace_id;
  r.artifact_id = "match-report-" + opportunity_id;
  r.created_at = "2026-01-01T00:00:00Z";
  r.opportunity_id = opportunity_id;
  r.version = "0.3";

  domain::RequirementDecision rd;
  rd.requirement_text = "C++20";
  rd.atom_id = "atom-001";
  rd.evidence_tokens = {"cpp", "cpp20"};
  r.requirement_decisions.push_back(rd);

  r.retrieval_stats.lexical_candidates = 3;
  r.retrieval_stats.embedding_candidates = 2;
  r.retrieval_stats.merged_candidates = 4;

  r.validation_summary.status = "accepted";
  r.validation_summary.finding_count = 0;
  r.validation_summary.fail_count = 0;
  r.validation_summary.warn_count = 0;

  return r;
}

TEST_CASE("upsert and get roundtrip", "[decision-store][sqlite]") {
  auto db = make_db();
  storage::sqlite::SqliteDecisionStore store(db);

  auto rec = make_record("decision-001", "trace-001");
  store.upsert(rec);

  auto retrieved = store.get("decision-001");
  REQUIRE(retrieved.has_value());
  CHECK(retrieved->decision_id == "decision-001");
  CHECK(retrieved->trace_id == "trace-001");
  CHECK(retrieved->artifact_id == "match-report-opp-001");
  CHECK(retrieved->opportunity_id == "opp-001");
  CHECK(retrieved->version == "0.3");
  REQUIRE(retrieved->created_at.has_value());
  CHECK(retrieved->created_at.value() == "2026-01-01T00:00:00Z");

  REQUIRE(retrieved->requirement_decisions.size() == 1);
  CHECK(retrieved->requirement_decisions[0].requirement_text == "C++20");
  REQUIRE(retrieved->requirement_decisions[0].atom_id.has_value());
  CHECK(retrieved->requirement_decisions[0].atom_id.value() == "atom-001");

  CHECK(retrieved->retrieval_stats.lexical_candidates == 3);
  CHECK(retrieved->retrieval_stats.merged_candidates == 4);
  CHECK(retrieved->validation_summary.status == "accepted");
}

TEST_CASE("get returns nullopt for missing decision", "[decision-store][sqlite]") {
  auto db = make_db();
  storage::sqlite::SqliteDecisionStore store(db);

  auto result = store.get("nonexistent-id");
  CHECK(!result.has_value());
}

TEST_CASE("upsert updates existing record", "[decision-store][sqlite]") {
  auto db = make_db();
  storage::sqlite::SqliteDecisionStore store(db);

  auto rec = make_record("decision-001", "trace-001");
  store.upsert(rec);

  // Modify and upsert again
  rec.validation_summary.status = "rejected";
  rec.validation_summary.fail_count = 1;
  store.upsert(rec);

  auto retrieved = store.get("decision-001");
  REQUIRE(retrieved.has_value());
  CHECK(retrieved->validation_summary.status == "rejected");
  CHECK(retrieved->validation_summary.fail_count == 1);
}

TEST_CASE("list_by_trace returns records ordered by decision_id", "[decision-store][sqlite]") {
  auto db = make_db();
  storage::sqlite::SqliteDecisionStore store(db);

  // Insert out of order
  store.upsert(make_record("decision-003", "trace-A"));
  store.upsert(make_record("decision-001", "trace-A"));
  store.upsert(make_record("decision-002", "trace-A"));

  auto records = store.list_by_trace("trace-A");
  REQUIRE(records.size() == 3);
  CHECK(records[0].decision_id == "decision-001");
  CHECK(records[1].decision_id == "decision-002");
  CHECK(records[2].decision_id == "decision-003");
}

TEST_CASE("list_by_trace only returns records for matching trace", "[decision-store][sqlite]") {
  auto db = make_db();
  storage::sqlite::SqliteDecisionStore store(db);

  store.upsert(make_record("decision-001", "trace-A"));
  store.upsert(make_record("decision-002", "trace-B"));
  store.upsert(make_record("decision-003", "trace-A"));

  auto records_a = store.list_by_trace("trace-A");
  REQUIRE(records_a.size() == 2);
  CHECK(records_a[0].decision_id == "decision-001");
  CHECK(records_a[1].decision_id == "decision-003");

  auto records_b = store.list_by_trace("trace-B");
  REQUIRE(records_b.size() == 1);
  CHECK(records_b[0].decision_id == "decision-002");

  auto records_c = store.list_by_trace("trace-C");
  CHECK(records_c.empty());
}

TEST_CASE("null created_at roundtrips correctly", "[decision-store][sqlite]") {
  auto db = make_db();
  storage::sqlite::SqliteDecisionStore store(db);

  auto rec = make_record("decision-001", "trace-001");
  rec.created_at = std::nullopt;
  store.upsert(rec);

  auto retrieved = store.get("decision-001");
  REQUIRE(retrieved.has_value());
  CHECK(!retrieved->created_at.has_value());
}

TEST_CASE("null atom_id roundtrips correctly", "[decision-store][sqlite]") {
  auto db = make_db();
  storage::sqlite::SqliteDecisionStore store(db);

  auto rec = make_record("decision-001", "trace-001");
  rec.requirement_decisions[0].atom_id = std::nullopt;
  store.upsert(rec);

  auto retrieved = store.get("decision-001");
  REQUIRE(retrieved.has_value());
  REQUIRE(retrieved->requirement_decisions.size() == 1);
  CHECK(!retrieved->requirement_decisions[0].atom_id.has_value());
}
