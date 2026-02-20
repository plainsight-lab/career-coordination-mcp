#include "ccmcp/storage/sqlite/sqlite_audit_log.h"
#include "ccmcp/storage/sqlite/sqlite_db.h"

#include <catch2/catch_test_macros.hpp>

using namespace ccmcp;

TEST_CASE("SqliteAuditLog append and query", "[sqlite][audit]") {
  // Create in-memory database
  auto db_result = storage::sqlite::SqliteDb::open(":memory:");
  REQUIRE(db_result.has_value());
  auto db = db_result.value();

  // Initialize schema
  auto schema_result = db->ensure_schema_v8();
  REQUIRE(schema_result.has_value());

  // Create audit log
  storage::sqlite::SqliteAuditLog audit_log(db);

  // Append 3 events with same trace_id
  std::string trace_id = "trace-001";
  audit_log.append(
      {"evt-001", trace_id, "RunStarted", R"({"test":true})", "2026-01-01T00:00:00Z", {}});
  audit_log.append({"evt-002",
                    trace_id,
                    "MatchCompleted",
                    R"({"score":0.5})",
                    "2026-01-01T00:00:01Z",
                    {"opp-001"}});
  audit_log.append(
      {"evt-003", trace_id, "RunCompleted", R"({"status":"success"})", "2026-01-01T00:00:02Z", {}});

  // Query by trace_id
  auto events = audit_log.query(trace_id);
  REQUIRE(events.size() == 3);

  // Verify deterministic ordering (by idx)
  CHECK(events[0].event_id == "evt-001");
  CHECK(events[1].event_id == "evt-002");
  CHECK(events[2].event_id == "evt-003");

  // Verify refs are preserved
  CHECK(events[0].refs.empty());
  CHECK(events[1].refs.size() == 1);
  CHECK(events[1].refs[0] == "opp-001");
}

TEST_CASE("SqliteAuditLog multiple traces", "[sqlite][audit]") {
  auto db_result = storage::sqlite::SqliteDb::open(":memory:");
  REQUIRE(db_result.has_value());
  auto db = db_result.value();
  db->ensure_schema_v8();

  storage::sqlite::SqliteAuditLog audit_log(db);

  // Append events for two different traces
  audit_log.append({"evt-1a", "trace-A", "Event1", "{}", "2026-01-01T00:00:00Z", {}});
  audit_log.append({"evt-1b", "trace-B", "Event1", "{}", "2026-01-01T00:00:00Z", {}});
  audit_log.append({"evt-2a", "trace-A", "Event2", "{}", "2026-01-01T00:00:01Z", {}});

  // Query trace-A
  auto events_a = audit_log.query("trace-A");
  REQUIRE(events_a.size() == 2);
  CHECK(events_a[0].event_id == "evt-1a");
  CHECK(events_a[1].event_id == "evt-2a");

  // Query trace-B
  auto events_b = audit_log.query("trace-B");
  REQUIRE(events_b.size() == 1);
  CHECK(events_b[0].event_id == "evt-1b");
}
