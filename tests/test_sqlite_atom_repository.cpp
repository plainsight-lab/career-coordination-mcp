#include "ccmcp/storage/sqlite/sqlite_atom_repository.h"
#include "ccmcp/storage/sqlite/sqlite_db.h"

#include <catch2/catch_test_macros.hpp>

using namespace ccmcp;

TEST_CASE("SqliteAtomRepository roundtrip", "[sqlite][repository]") {
  // Create in-memory database
  auto db_result = storage::sqlite::SqliteDb::open(":memory:");
  REQUIRE(db_result.has_value());
  auto db = db_result.value();

  // Initialize schema
  auto schema_result = db->ensure_schema_v1();
  REQUIRE(schema_result.has_value());

  // Create repository
  storage::sqlite::SqliteAtomRepository repo(db);

  // Create test atom
  domain::ExperienceAtom atom{core::AtomId{"atom-001"},
                              "cpp",
                              "Modern C++",
                              "Built C++20 systems",
                              {"cpp20", "systems"},
                              true,
                              {"https://example.com/evidence"}};

  // Upsert
  repo.upsert(atom);

  // Get
  auto retrieved = repo.get(core::AtomId{"atom-001"});
  REQUIRE(retrieved.has_value());
  CHECK(retrieved->atom_id.value == "atom-001");
  CHECK(retrieved->domain == "cpp");
  CHECK(retrieved->title == "Modern C++");
  CHECK(retrieved->claim == "Built C++20 systems");
  CHECK(retrieved->tags.size() == 2);
  CHECK(retrieved->verified == true);
  CHECK(retrieved->evidence_refs.size() == 1);
}

TEST_CASE("SqliteAtomRepository list_verified orders deterministically", "[sqlite][repository]") {
  auto db_result = storage::sqlite::SqliteDb::open(":memory:");
  REQUIRE(db_result.has_value());
  auto db = db_result.value();
  db->ensure_schema_v1();

  storage::sqlite::SqliteAtomRepository repo(db);

  // Insert out of order
  repo.upsert({core::AtomId{"atom-003"}, "go", "Go", "Claim", {}, true, {}});
  repo.upsert({core::AtomId{"atom-001"}, "cpp", "C++", "Claim", {}, true, {}});
  repo.upsert({core::AtomId{"atom-002"}, "rust", "Rust", "Claim", {}, false, {}});

  auto verified = repo.list_verified();
  REQUIRE(verified.size() == 2);
  // Should be sorted by AtomId (lexicographic)
  CHECK(verified[0].atom_id.value == "atom-001");
  CHECK(verified[1].atom_id.value == "atom-003");
}

TEST_CASE("SqliteAtomRepository upsert replaces existing", "[sqlite][repository]") {
  auto db_result = storage::sqlite::SqliteDb::open(":memory:");
  REQUIRE(db_result.has_value());
  auto db = db_result.value();
  db->ensure_schema_v1();

  storage::sqlite::SqliteAtomRepository repo(db);

  // First upsert
  repo.upsert({core::AtomId{"atom-001"}, "cpp", "Title 1", "Claim 1", {}, false, {}});

  // Second upsert with same ID
  repo.upsert({core::AtomId{"atom-001"}, "rust", "Title 2", "Claim 2", {}, true, {}});

  auto retrieved = repo.get(core::AtomId{"atom-001"});
  REQUIRE(retrieved.has_value());
  CHECK(retrieved->domain == "rust");
  CHECK(retrieved->title == "Title 2");
  CHECK(retrieved->verified == true);
}
