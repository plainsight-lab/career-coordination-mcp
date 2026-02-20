#include "ccmcp/core/sha256.h"
#include "ccmcp/domain/runtime_config_snapshot.h"
#include "ccmcp/storage/sqlite/sqlite_db.h"
#include "ccmcp/storage/sqlite/sqlite_runtime_snapshot_store.h"

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

// ── SHA-256 correctness ────────────────────────────────────────────────────

TEST_CASE("sha256_hex: empty string produces known FIPS 180-4 digest", "[sha256]") {
  // FIPS 180-4 NIST test vector: SHA-256 of the empty string.
  const std::string digest = core::sha256_hex("");
  CHECK(digest == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
  CHECK(digest.size() == 64);
}

TEST_CASE("sha256_hex: \"abc\" produces known digest", "[sha256]") {
  // Verified against OpenSSL, shasum, and Python hashlib.
  // SHA-256 of the 3-byte ASCII string "abc" (0x61 0x62 0x63).
  const std::string digest = core::sha256_hex("abc");
  CHECK(digest == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  CHECK(digest.size() == 64);
}

TEST_CASE("sha256_hex: same input always produces the same output", "[sha256]") {
  const std::string input = "determinism check";
  const std::string d1 = core::sha256_hex(input);
  const std::string d2 = core::sha256_hex(input);
  CHECK(d1 == d2);
  CHECK(d1.size() == 64);
}

TEST_CASE("sha256_hex: different inputs produce different digests", "[sha256]") {
  CHECK(core::sha256_hex("hello") != core::sha256_hex("world"));
  CHECK(core::sha256_hex("A") != core::sha256_hex("a"));
  CHECK(core::sha256_hex("") != core::sha256_hex(" "));
}

// ── RuntimeConfigSnapshot serialization ───────────────────────────────────

TEST_CASE("to_json/from_json: roundtrip preserves all fields", "[snapshot][serialization]") {
  domain::RuntimeConfigSnapshot snap;
  snap.snapshot_format_version = 2;
  snap.db_schema_version = 8;
  snap.vector_backend = "sqlite";
  snap.redis_host = "localhost";
  snap.redis_port = 6379;
  snap.redis_db = 2;
  snap.build_version = "0.4";

  const std::string json_str = domain::to_json(snap);
  const domain::RuntimeConfigSnapshot restored = domain::from_json(json_str);

  CHECK(restored.snapshot_format_version == 2);
  CHECK(restored.db_schema_version == 8);
  CHECK(restored.vector_backend == "sqlite");
  CHECK(restored.redis_host == "localhost");
  CHECK(restored.redis_port == 6379);
  CHECK(restored.redis_db == 2);
  CHECK(restored.build_version == "0.4");
  CHECK(restored.feature_flags.empty());
}

TEST_CASE("to_json: keys appear in alphabetical order", "[snapshot][serialization]") {
  domain::RuntimeConfigSnapshot snap;
  snap.vector_backend = "inmemory";
  snap.redis_host = "127.0.0.1";
  snap.build_version = "0.4";
  snap.db_schema_version = 8;

  const std::string json_str = domain::to_json(snap);

  // Verify the alphabetical key ordering in the serialized JSON.
  // Keys: build_version < db_schema_version < feature_flags < redis_db < redis_host
  //       < redis_port < snapshot_format_version < vector_backend
  const auto pos_build = json_str.find("\"build_version\"");
  const auto pos_dbschema = json_str.find("\"db_schema_version\"");
  const auto pos_flags = json_str.find("\"feature_flags\"");
  const auto pos_rdb = json_str.find("\"redis_db\"");
  const auto pos_rhost = json_str.find("\"redis_host\"");
  const auto pos_rport = json_str.find("\"redis_port\"");
  const auto pos_snapver = json_str.find("\"snapshot_format_version\"");
  const auto pos_vector = json_str.find("\"vector_backend\"");

  REQUIRE(pos_build != std::string::npos);
  REQUIRE(pos_dbschema != std::string::npos);
  REQUIRE(pos_flags != std::string::npos);
  REQUIRE(pos_rdb != std::string::npos);
  REQUIRE(pos_rhost != std::string::npos);
  REQUIRE(pos_rport != std::string::npos);
  REQUIRE(pos_snapver != std::string::npos);
  REQUIRE(pos_vector != std::string::npos);

  CHECK(pos_build < pos_dbschema);
  CHECK(pos_dbschema < pos_flags);
  CHECK(pos_flags < pos_rdb);
  CHECK(pos_rdb < pos_rhost);
  CHECK(pos_rhost < pos_rport);
  CHECK(pos_rport < pos_snapver);
  CHECK(pos_snapver < pos_vector);
}

TEST_CASE("to_json: feature_flags serialized correctly when non-empty",
          "[snapshot][serialization]") {
  domain::RuntimeConfigSnapshot snap;
  snap.feature_flags["enable_hybrid"] = "true";
  snap.feature_flags["log_level"] = "debug";

  const std::string json_str = domain::to_json(snap);
  const domain::RuntimeConfigSnapshot restored = domain::from_json(json_str);

  REQUIRE(restored.feature_flags.size() == 2);
  CHECK(restored.feature_flags.at("enable_hybrid") == "true");
  CHECK(restored.feature_flags.at("log_level") == "debug");
}

// ── SqliteRuntimeSnapshotStore ─────────────────────────────────────────────

TEST_CASE("SqliteRuntimeSnapshotStore: save and get_snapshot_json roundtrip",
          "[snapshot][sqlite]") {
  auto db = make_db();
  storage::sqlite::SqliteRuntimeSnapshotStore store(db);

  domain::RuntimeConfigSnapshot snap;
  snap.snapshot_format_version = 2;
  snap.db_schema_version = 8;
  snap.vector_backend = "sqlite";
  snap.redis_host = "127.0.0.1";
  snap.redis_port = 6379;
  snap.redis_db = 0;
  snap.build_version = "0.4";

  const std::string snap_json = domain::to_json(snap);
  const std::string snap_hash = core::sha256_hex(snap_json);

  store.save("run-001", snap_json, snap_hash, "2026-01-01T00:00:00Z");

  const auto retrieved = store.get_snapshot_json("run-001");
  REQUIRE(retrieved.has_value());

  const domain::RuntimeConfigSnapshot restored = domain::from_json(retrieved.value());
  CHECK(restored.snapshot_format_version == 2);
  CHECK(restored.db_schema_version == 8);
  CHECK(restored.vector_backend == "sqlite");
  CHECK(restored.redis_host == "127.0.0.1");
  CHECK(restored.redis_port == 6379);
  CHECK(restored.redis_db == 0);
  CHECK(restored.build_version == "0.4");
}

TEST_CASE("SqliteRuntimeSnapshotStore: get_snapshot_json returns nullopt for missing run_id",
          "[snapshot][sqlite]") {
  auto db = make_db();
  storage::sqlite::SqliteRuntimeSnapshotStore store(db);

  const auto result = store.get_snapshot_json("nonexistent-run");
  CHECK(!result.has_value());
}
