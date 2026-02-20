#include "ccmcp/storage/audit_chain.h"
#include "ccmcp/storage/audit_log.h"
#include "ccmcp/storage/sqlite/sqlite_audit_log.h"
#include "ccmcp/storage/sqlite/sqlite_db.h"

#include <catch2/catch_test_macros.hpp>

using namespace ccmcp;

// Helper: open an in-memory DB with schema v8 applied.
static std::shared_ptr<storage::sqlite::SqliteDb> make_db() {
  auto result = storage::sqlite::SqliteDb::open(":memory:");
  REQUIRE(result.has_value());
  auto db = result.value();
  auto schema = db->ensure_schema_v8();
  REQUIRE(schema.has_value());
  return db;
}

// Helper: build a minimal AuditEvent (hash fields left empty; filled by append).
static storage::AuditEvent make_event(const std::string& event_id, const std::string& trace_id,
                                      const std::string& event_type = "TestEvent",
                                      const std::string& payload = "{}") {
  return {event_id, trace_id, event_type, payload, "2026-01-01T00:00:00Z", {}};
}

// ── compute_event_hash ──────────────────────────────────────────────────────

TEST_CASE("compute_event_hash: same input always produces same digest", "[audit_chain]") {
  const storage::AuditEvent ev = make_event("evt-1", "trace-A");
  const std::string prev = std::string(storage::kGenesisHash);

  const std::string h1 = storage::compute_event_hash(ev, prev);
  const std::string h2 = storage::compute_event_hash(ev, prev);

  CHECK(h1 == h2);
  CHECK(h1.size() == 64);
}

TEST_CASE("compute_event_hash: different previous_hash produces different digest",
          "[audit_chain]") {
  const storage::AuditEvent ev = make_event("evt-1", "trace-A");
  const std::string prev1 = std::string(storage::kGenesisHash);
  const std::string prev2(64, 'f');  // all-f — different from genesis

  CHECK(storage::compute_event_hash(ev, prev1) != storage::compute_event_hash(ev, prev2));
}

// ── verify_audit_chain ──────────────────────────────────────────────────────

TEST_CASE("verify_audit_chain: empty chain is valid", "[audit_chain]") {
  const auto result = storage::verify_audit_chain({});
  CHECK(result.valid);
  CHECK(result.first_invalid_index == 0);
  CHECK(result.error.empty());
}

TEST_CASE("verify_audit_chain: valid chain verifies successfully", "[audit_chain]") {
  // Build a 3-event chain manually using InMemoryAuditLog
  storage::InMemoryAuditLog log;
  log.append(make_event("evt-1", "t1", "RunStarted"));
  log.append(make_event("evt-2", "t1", "MatchCompleted"));
  log.append(make_event("evt-3", "t1", "RunCompleted"));

  const auto events = log.query("t1");
  REQUIRE(events.size() == 3);

  const auto result = storage::verify_audit_chain(events);
  CHECK(result.valid);
  CHECK(result.first_invalid_index == 3);
  CHECK(result.error.empty());
}

TEST_CASE("verify_audit_chain: single-event mutation breaks verification", "[audit_chain]") {
  storage::InMemoryAuditLog log;
  log.append(make_event("evt-1", "t1"));
  log.append(make_event("evt-2", "t1"));
  log.append(make_event("evt-3", "t1"));

  auto events = log.query("t1");
  REQUIRE(events.size() == 3);

  // Tamper with the payload of the second event (event_hash becomes stale)
  events[1].payload = R"({"tampered":true})";

  const auto result = storage::verify_audit_chain(events);
  CHECK(!result.valid);
  // Event at index 1 has a stale event_hash (hash was computed on original payload)
  CHECK(result.first_invalid_index == 1);
}

TEST_CASE("verify_audit_chain: reordered events break verification", "[audit_chain]") {
  storage::InMemoryAuditLog log;
  log.append(make_event("evt-1", "t1"));
  log.append(make_event("evt-2", "t1"));
  log.append(make_event("evt-3", "t1"));

  auto events = log.query("t1");
  REQUIRE(events.size() == 3);

  // Swap events[0] and events[1] — the previous_hash chain breaks
  std::swap(events[0], events[1]);

  const auto result = storage::verify_audit_chain(events);
  CHECK(!result.valid);
  // First event (now formerly evt-2) expects genesis as previous_hash, but has evt-1's hash
  CHECK(result.first_invalid_index == 0);
}

TEST_CASE("verify_audit_chain: identical event stream produces identical hash chain",
          "[audit_chain]") {
  auto build_chain = [] {
    storage::InMemoryAuditLog log;
    log.append(make_event("evt-1", "t1", "EventA", R"({"x":1})"));
    log.append(make_event("evt-2", "t1", "EventB", R"({"x":2})"));
    return log.query("t1");
  };

  const auto chain1 = build_chain();
  const auto chain2 = build_chain();

  REQUIRE(chain1.size() == 2);
  REQUIRE(chain2.size() == 2);

  CHECK(chain1[0].event_hash == chain2[0].event_hash);
  CHECK(chain1[1].event_hash == chain2[1].event_hash);
  CHECK(chain1[1].previous_hash == chain2[1].previous_hash);
}

// ── SqliteAuditLog hash chain ───────────────────────────────────────────────

TEST_CASE("SqliteAuditLog: appended events carry computed hashes", "[audit_chain][sqlite]") {
  auto db = make_db();
  storage::sqlite::SqliteAuditLog audit_log(db);

  audit_log.append(make_event("evt-1", "t1", "RunStarted"));
  audit_log.append(make_event("evt-2", "t1", "RunCompleted"));

  const auto events = audit_log.query("t1");
  REQUIRE(events.size() == 2);

  // First event must chain from genesis
  CHECK(events[0].previous_hash == std::string(storage::kGenesisHash));
  CHECK(events[0].event_hash.size() == 64);

  // Second event must chain from first event's hash
  CHECK(events[1].previous_hash == events[0].event_hash);
  CHECK(events[1].event_hash.size() == 64);
  CHECK(events[1].event_hash != events[0].event_hash);
}

TEST_CASE("SqliteAuditLog: verify_audit_chain validates persisted chain", "[audit_chain][sqlite]") {
  auto db = make_db();
  storage::sqlite::SqliteAuditLog audit_log(db);

  audit_log.append(make_event("evt-1", "t1", "RunStarted"));
  audit_log.append(make_event("evt-2", "t1", "MatchCompleted", R"({"score":0.8})"));
  audit_log.append(make_event("evt-3", "t1", "RunCompleted"));

  const auto events = audit_log.query("t1");
  REQUIRE(events.size() == 3);

  const auto result = storage::verify_audit_chain(events);
  CHECK(result.valid);
  CHECK(result.first_invalid_index == 3);
}
