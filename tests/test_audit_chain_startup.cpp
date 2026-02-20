#include "ccmcp/storage/audit_chain.h"
#include "ccmcp/storage/audit_log.h"
#include "ccmcp/storage/sqlite/sqlite_audit_log.h"
#include "ccmcp/storage/sqlite/sqlite_db.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>
#include <vector>

using namespace ccmcp;

// Helper: build a minimal AuditEvent with the given trace and id.
static storage::AuditEvent make_event(const std::string& trace_id, const std::string& event_id) {
  storage::AuditEvent e;
  e.event_id = event_id;
  e.trace_id = trace_id;
  e.event_type = "TestEvent";
  e.payload = "{}";
  e.created_at = "2026-01-01T00:00:00Z";
  return e;
}

// Helper: open an in-memory DB with schema v8 applied.
static std::shared_ptr<storage::sqlite::SqliteDb> make_db() {
  auto result = storage::sqlite::SqliteDb::open(":memory:");
  REQUIRE(result.has_value());
  auto db = result.value();
  auto schema = db->ensure_schema_v8();
  REQUIRE(schema.has_value());
  return db;
}

// ── list_trace_ids: InMemoryAuditLog ──────────────────────────────────────

TEST_CASE("list_trace_ids: empty InMemoryAuditLog returns empty list", "[audit-chain]") {
  storage::InMemoryAuditLog log;
  const auto ids = log.list_trace_ids();
  CHECK(ids.empty());
}

TEST_CASE("list_trace_ids: single-trace log returns one ID", "[audit-chain]") {
  storage::InMemoryAuditLog log;
  log.append(make_event("trace-alpha", "evt-001"));
  log.append(make_event("trace-alpha", "evt-002"));

  const auto ids = log.list_trace_ids();
  REQUIRE(ids.size() == 1);
  CHECK(ids[0] == "trace-alpha");
}

TEST_CASE("list_trace_ids: multi-trace log returns all distinct IDs", "[audit-chain]") {
  storage::InMemoryAuditLog log;
  log.append(make_event("trace-a", "evt-a1"));
  log.append(make_event("trace-b", "evt-b1"));
  log.append(make_event("trace-c", "evt-c1"));
  log.append(make_event("trace-a", "evt-a2"));  // duplicate trace — must not produce duplicates

  const auto ids = log.list_trace_ids();
  REQUIRE(ids.size() == 3);

  // Order is std::map iteration order (alphabetical for string keys).
  std::vector<std::string> sorted = ids;
  std::sort(sorted.begin(), sorted.end());
  CHECK(sorted[0] == "trace-a");
  CHECK(sorted[1] == "trace-b");
  CHECK(sorted[2] == "trace-c");
}

// ── list_trace_ids: SqliteAuditLog ────────────────────────────────────────

TEST_CASE("list_trace_ids: SqliteAuditLog returns correct trace IDs", "[audit-chain]") {
  auto db = make_db();
  storage::sqlite::SqliteAuditLog log(db);

  log.append(make_event("sq-trace-x", "sq-evt-x1"));
  log.append(make_event("sq-trace-y", "sq-evt-y1"));
  log.append(make_event("sq-trace-x", "sq-evt-x2"));

  const auto ids = log.list_trace_ids();
  REQUIRE(ids.size() == 2);

  std::vector<std::string> sorted = ids;
  std::sort(sorted.begin(), sorted.end());
  CHECK(sorted[0] == "sq-trace-x");
  CHECK(sorted[1] == "sq-trace-y");
}

// ── verify_audit_chain on a valid InMemoryAuditLog chain ──────────────────

TEST_CASE("verify_audit_chain: valid InMemoryAuditLog chain reports clean", "[audit-chain]") {
  storage::InMemoryAuditLog log;
  log.append(make_event("chain-trace", "chain-evt-1"));
  log.append(make_event("chain-trace", "chain-evt-2"));
  log.append(make_event("chain-trace", "chain-evt-3"));

  const auto events = log.query("chain-trace");
  REQUIRE(events.size() == 3);

  const auto result = storage::verify_audit_chain(events);
  CHECK(result.valid);
  CHECK(result.first_invalid_index == events.size());
  CHECK(result.error.empty());
}
