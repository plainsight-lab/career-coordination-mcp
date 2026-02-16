#include "ccmcp/storage/inmemory_opportunity_repository.h"

#include <catch2/catch_test_macros.hpp>

using namespace ccmcp;

TEST_CASE("InMemoryOpportunityRepository::upsert stores opportunity", "[storage][repository]") {
  storage::InMemoryOpportunityRepository repo;
  domain::Opportunity opp{
      core::OpportunityId{"opp-001"}, "ExampleCo", "Principal Architect", {}, "manual"};

  repo.upsert(opp);

  auto retrieved = repo.get(core::OpportunityId{"opp-001"});
  REQUIRE(retrieved.has_value());
  CHECK(retrieved->opportunity_id.value == "opp-001");
  CHECK(retrieved->company == "ExampleCo");
  CHECK(retrieved->role_title == "Principal Architect");
}

TEST_CASE("InMemoryOpportunityRepository::upsert replaces existing opportunity",
          "[storage][repository]") {
  storage::InMemoryOpportunityRepository repo;
  domain::Opportunity opp1{core::OpportunityId{"opp-001"}, "Company1", "Title1", {}, "source1"};
  domain::Opportunity opp2{core::OpportunityId{"opp-001"}, "Company2", "Title2", {}, "source2"};

  repo.upsert(opp1);
  repo.upsert(opp2);

  auto retrieved = repo.get(core::OpportunityId{"opp-001"});
  REQUIRE(retrieved.has_value());
  CHECK(retrieved->company == "Company2");
  CHECK(retrieved->role_title == "Title2");
}

TEST_CASE("InMemoryOpportunityRepository::get returns nullopt for missing opportunity",
          "[storage][repository]") {
  storage::InMemoryOpportunityRepository repo;

  auto result = repo.get(core::OpportunityId{"nonexistent"});
  CHECK_FALSE(result.has_value());
}

TEST_CASE("InMemoryOpportunityRepository::list_all returns opportunities in deterministic order",
          "[storage][repository]") {
  storage::InMemoryOpportunityRepository repo;
  // Insert out of order
  repo.upsert({core::OpportunityId{"opp-003"}, "C", "Title", {}, ""});
  repo.upsert({core::OpportunityId{"opp-001"}, "A", "Title", {}, ""});
  repo.upsert({core::OpportunityId{"opp-002"}, "B", "Title", {}, ""});

  auto all_opps = repo.list_all();
  REQUIRE(all_opps.size() == 3);
  // Should be sorted by OpportunityId (lexicographic)
  CHECK(all_opps[0].opportunity_id.value == "opp-001");
  CHECK(all_opps[1].opportunity_id.value == "opp-002");
  CHECK(all_opps[2].opportunity_id.value == "opp-003");
}
