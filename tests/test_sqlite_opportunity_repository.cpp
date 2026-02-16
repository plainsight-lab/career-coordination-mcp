#include "ccmcp/storage/sqlite/sqlite_db.h"
#include "ccmcp/storage/sqlite/sqlite_opportunity_repository.h"

#include <catch2/catch_test_macros.hpp>

using namespace ccmcp;

TEST_CASE("SqliteOpportunityRepository roundtrip with requirements", "[sqlite][repository]") {
  // Create in-memory database
  auto db_result = storage::sqlite::SqliteDb::open(":memory:");
  REQUIRE(db_result.has_value());
  auto db = db_result.value();

  // Initialize schema
  auto schema_result = db->ensure_schema_v1();
  REQUIRE(schema_result.has_value());

  // Create repository
  storage::sqlite::SqliteOpportunityRepository repo(db);

  // Create test opportunity with requirements
  domain::Opportunity opp{core::OpportunityId{"opp-001"},
                          "ExampleCo",
                          "Principal Architect",
                          {
                              domain::Requirement{"C++20", {"cpp", "cpp20"}, true},
                              domain::Requirement{"Architecture", {"architecture"}, true},
                              domain::Requirement{"Leadership", {"leadership"}, false},
                          },
                          "manual"};

  // Upsert
  repo.upsert(opp);

  // Get
  auto retrieved = repo.get(core::OpportunityId{"opp-001"});
  REQUIRE(retrieved.has_value());
  CHECK(retrieved->opportunity_id.value == "opp-001");
  CHECK(retrieved->company == "ExampleCo");
  CHECK(retrieved->role_title == "Principal Architect");
  CHECK(retrieved->source == "manual");

  // Verify requirements order is preserved
  REQUIRE(retrieved->requirements.size() == 3);
  CHECK(retrieved->requirements[0].text == "C++20");
  CHECK(retrieved->requirements[1].text == "Architecture");
  CHECK(retrieved->requirements[2].text == "Leadership");
  CHECK(retrieved->requirements[2].required == false);
}

TEST_CASE("SqliteOpportunityRepository upsert updates requirements", "[sqlite][repository]") {
  auto db_result = storage::sqlite::SqliteDb::open(":memory:");
  REQUIRE(db_result.has_value());
  auto db = db_result.value();
  db->ensure_schema_v1();

  storage::sqlite::SqliteOpportunityRepository repo(db);

  // First upsert with 2 requirements
  domain::Opportunity opp1{core::OpportunityId{"opp-001"},
                           "Company1",
                           "Role1",
                           {
                               domain::Requirement{"Req1", {"tag1"}, true},
                               domain::Requirement{"Req2", {"tag2"}, false},
                           },
                           "source1"};
  repo.upsert(opp1);

  // Second upsert with 3 requirements (should replace)
  domain::Opportunity opp2{core::OpportunityId{"opp-001"},
                           "Company2",
                           "Role2",
                           {
                               domain::Requirement{"NewReq1", {"tag1"}, true},
                               domain::Requirement{"NewReq2", {"tag2"}, true},
                               domain::Requirement{"NewReq3", {"tag3"}, false},
                           },
                           "source2"};
  repo.upsert(opp2);

  auto retrieved = repo.get(core::OpportunityId{"opp-001"});
  REQUIRE(retrieved.has_value());
  CHECK(retrieved->company == "Company2");
  REQUIRE(retrieved->requirements.size() == 3);
  CHECK(retrieved->requirements[0].text == "NewReq1");
  CHECK(retrieved->requirements[2].text == "NewReq3");
}

TEST_CASE("SqliteOpportunityRepository list_all orders deterministically", "[sqlite][repository]") {
  auto db_result = storage::sqlite::SqliteDb::open(":memory:");
  REQUIRE(db_result.has_value());
  auto db = db_result.value();
  db->ensure_schema_v1();

  storage::sqlite::SqliteOpportunityRepository repo(db);

  // Insert out of order
  repo.upsert({core::OpportunityId{"opp-003"}, "C", "Role", {}, ""});
  repo.upsert({core::OpportunityId{"opp-001"}, "A", "Role", {}, ""});
  repo.upsert({core::OpportunityId{"opp-002"}, "B", "Role", {}, ""});

  auto all = repo.list_all();
  REQUIRE(all.size() == 3);
  // Should be sorted by OpportunityId (lexicographic)
  CHECK(all[0].opportunity_id.value == "opp-001");
  CHECK(all[1].opportunity_id.value == "opp-002");
  CHECK(all[2].opportunity_id.value == "opp-003");
}
