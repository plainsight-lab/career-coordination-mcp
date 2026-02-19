#include "ccmcp/indexing/index_run.h"
#include "ccmcp/storage/sqlite/sqlite_db.h"
#include "ccmcp/storage/sqlite/sqlite_index_run_store.h"

#include <catch2/catch_test_macros.hpp>

using namespace ccmcp;

// Helper: open an in-memory DB with schema v6 applied (chained: v1→v6).
static std::shared_ptr<storage::sqlite::SqliteDb> make_db() {
  auto result = storage::sqlite::SqliteDb::open(":memory:");
  REQUIRE(result.has_value());
  auto db = result.value();
  auto schema = db->ensure_schema_v6();
  REQUIRE(schema.has_value());
  return db;
}

// Helper: construct a minimal completed IndexRun.
static indexing::IndexRun make_run(
    const std::string& run_id,
    indexing::IndexRunStatus status = indexing::IndexRunStatus::kCompleted) {
  return {run_id,
          "2026-01-01T00:00:00Z",
          "2026-01-01T00:01:00Z",
          "deterministic-stub",
          "",
          "",
          status,
          R"({"indexed":0,"skipped":0,"stale":0,"scope":"all"})"};
}

TEST_CASE("upsert_run and get_run roundtrip", "[indexing][sqlite][index-run-store]") {
  auto db = make_db();
  storage::sqlite::SqliteIndexRunStore store(db);

  auto run = make_run("run-001");
  store.upsert_run(run);

  auto retrieved = store.get_run("run-001");
  REQUIRE(retrieved.has_value());
  CHECK(retrieved->run_id == "run-001");
  CHECK(retrieved->provider_id == "deterministic-stub");
  CHECK(retrieved->model_id.empty());
  CHECK(retrieved->prompt_version.empty());
  CHECK(retrieved->status == indexing::IndexRunStatus::kCompleted);
  REQUIRE(retrieved->started_at.has_value());
  CHECK(retrieved->started_at.value() == "2026-01-01T00:00:00Z");
  REQUIRE(retrieved->completed_at.has_value());
  CHECK(retrieved->completed_at.value() == "2026-01-01T00:01:00Z");
}

TEST_CASE("upsert_run updates existing run", "[indexing][sqlite][index-run-store]") {
  auto db = make_db();
  storage::sqlite::SqliteIndexRunStore store(db);

  // Insert as running, then update to completed.
  auto run = make_run("run-002", indexing::IndexRunStatus::kRunning);
  run.completed_at = std::nullopt;
  store.upsert_run(run);

  auto mid = store.get_run("run-002");
  REQUIRE(mid.has_value());
  CHECK(mid->status == indexing::IndexRunStatus::kRunning);
  CHECK(!mid->completed_at.has_value());

  // Update to completed.
  run.status = indexing::IndexRunStatus::kCompleted;
  run.completed_at = "2026-01-01T00:02:00Z";
  store.upsert_run(run);

  auto final_run = store.get_run("run-002");
  REQUIRE(final_run.has_value());
  CHECK(final_run->status == indexing::IndexRunStatus::kCompleted);
  REQUIRE(final_run->completed_at.has_value());
  CHECK(final_run->completed_at.value() == "2026-01-01T00:02:00Z");
}

TEST_CASE("list_runs returns deterministic order", "[indexing][sqlite][index-run-store]") {
  auto db = make_db();
  storage::sqlite::SqliteIndexRunStore store(db);

  // Insert out of alphabetical order.
  store.upsert_run(make_run("run-003"));
  store.upsert_run(make_run("run-001"));
  store.upsert_run(make_run("run-002"));

  auto runs = store.list_runs();
  REQUIRE(runs.size() == 3);
  CHECK(runs[0].run_id == "run-001");
  CHECK(runs[1].run_id == "run-002");
  CHECK(runs[2].run_id == "run-003");
}

TEST_CASE("upsert_entry and get_entries_for_run", "[indexing][sqlite][index-run-store]") {
  auto db = make_db();
  storage::sqlite::SqliteIndexRunStore store(db);

  store.upsert_run(make_run("run-001"));

  indexing::IndexEntry entry1{"run-001",    "atom",       "atom-a",
                              "hash-src-1", "hash-vec-1", "2026-01-01T00:00:10Z"};
  indexing::IndexEntry entry2{"run-001",    "atom",       "atom-b",
                              "hash-src-2", "hash-vec-2", "2026-01-01T00:00:11Z"};
  store.upsert_entry(entry1);
  store.upsert_entry(entry2);

  auto entries = store.get_entries_for_run("run-001");
  REQUIRE(entries.size() == 2);
  // Ordered by (artifact_type, artifact_id).
  CHECK(entries[0].artifact_id == "atom-a");
  CHECK(entries[0].source_hash == "hash-src-1");
  CHECK(entries[0].vector_hash == "hash-vec-1");
  REQUIRE(entries[0].indexed_at.has_value());
  CHECK(entries[0].indexed_at.value() == "2026-01-01T00:00:10Z");
  CHECK(entries[1].artifact_id == "atom-b");
}

TEST_CASE("get_last_source_hash returns nullopt for no prior run",
          "[indexing][sqlite][index-run-store]") {
  auto db = make_db();
  storage::sqlite::SqliteIndexRunStore store(db);

  auto result = store.get_last_source_hash("atom-001", "atom", "deterministic-stub", "", "");
  CHECK(!result.has_value());
}

TEST_CASE("get_last_source_hash returns hash from completed run",
          "[indexing][sqlite][index-run-store]") {
  auto db = make_db();
  storage::sqlite::SqliteIndexRunStore store(db);

  store.upsert_run(make_run("run-001"));
  store.upsert_entry({"run-001", "atom", "atom-x", "hash-abc", "vec-abc", std::nullopt});

  auto result = store.get_last_source_hash("atom-x", "atom", "deterministic-stub", "", "");
  REQUIRE(result.has_value());
  CHECK(result.value() == "hash-abc");
}

TEST_CASE("get_last_source_hash ignores non-completed runs",
          "[indexing][sqlite][index-run-store]") {
  auto db = make_db();
  storage::sqlite::SqliteIndexRunStore store(db);

  // Insert a running run with an entry.
  auto run = make_run("run-001", indexing::IndexRunStatus::kRunning);
  run.completed_at = std::nullopt;
  store.upsert_run(run);
  store.upsert_entry({"run-001", "atom", "atom-x", "hash-running", "vec-r", std::nullopt});

  auto result = store.get_last_source_hash("atom-x", "atom", "deterministic-stub", "", "");
  CHECK(!result.has_value());  // running run should be excluded
}

TEST_CASE("get_last_source_hash matches provider/model/prompt combination",
          "[indexing][sqlite][index-run-store]") {
  auto db = make_db();
  storage::sqlite::SqliteIndexRunStore store(db);

  // Completed run with provider A.
  indexing::IndexRun run_a{"run-a",
                           "2026-01-01T00:00:00Z",
                           "2026-01-01T00:01:00Z",
                           "provider-a",
                           "model-1",
                           "v1",
                           indexing::IndexRunStatus::kCompleted,
                           "{}"};
  store.upsert_run(run_a);
  store.upsert_entry({"run-a", "atom", "atom-x", "hash-a", "vec-a", std::nullopt});

  // Completed run with provider B.
  indexing::IndexRun run_b{"run-b",
                           "2026-01-01T00:02:00Z",
                           "2026-01-01T00:03:00Z",
                           "provider-b",
                           "model-1",
                           "v1",
                           indexing::IndexRunStatus::kCompleted,
                           "{}"};
  store.upsert_run(run_b);
  store.upsert_entry({"run-b", "atom", "atom-x", "hash-b", "vec-b", std::nullopt});

  // Query for provider-a only.
  auto result_a = store.get_last_source_hash("atom-x", "atom", "provider-a", "model-1", "v1");
  REQUIRE(result_a.has_value());
  CHECK(result_a.value() == "hash-a");

  // Query for provider-b only.
  auto result_b = store.get_last_source_hash("atom-x", "atom", "provider-b", "model-1", "v1");
  REQUIRE(result_b.has_value());
  CHECK(result_b.value() == "hash-b");

  // Query for an unknown provider returns nullopt.
  auto result_c = store.get_last_source_hash("atom-x", "atom", "provider-unknown", "model-1", "v1");
  CHECK(!result_c.has_value());
}

// ---------------------------------------------------------------------------
// v0.4 Slice 1 — Monotonic counter tests
// ---------------------------------------------------------------------------

TEST_CASE("next_index_run_id returns run-1 on first call", "[indexing][sqlite][index-run-store]") {
  auto db = make_db();
  storage::sqlite::SqliteIndexRunStore store(db);

  const std::string id = store.next_index_run_id();
  CHECK(id == "run-1");
}

TEST_CASE("next_index_run_id is strictly monotonically increasing",
          "[indexing][sqlite][index-run-store]") {
  auto db = make_db();
  storage::sqlite::SqliteIndexRunStore store(db);

  const std::string id1 = store.next_index_run_id();
  const std::string id2 = store.next_index_run_id();
  const std::string id3 = store.next_index_run_id();

  CHECK(id1 == "run-1");
  CHECK(id2 == "run-2");
  CHECK(id3 == "run-3");
}

TEST_CASE("next_index_run_id counter persists across separate store instances on the same db",
          "[indexing][sqlite][index-run-store]") {
  auto db = make_db();

  // First store instance allocates run-1.
  {
    storage::sqlite::SqliteIndexRunStore store1(db);
    CHECK(store1.next_index_run_id() == "run-1");
  }

  // Second store instance on the same connection continues from run-2.
  // This simulates the MCP server allocating a second run in the same session.
  {
    storage::sqlite::SqliteIndexRunStore store2(db);
    CHECK(store2.next_index_run_id() == "run-2");
  }
}
