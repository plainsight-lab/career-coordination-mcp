#include "ccmcp/storage/inmemory_interaction_repository.h"

#include <catch2/catch_test_macros.hpp>

using namespace ccmcp;

TEST_CASE("InMemoryInteractionRepository::upsert stores interaction", "[storage][repository]") {
  storage::InMemoryInteractionRepository repo;
  domain::Interaction interaction{core::InteractionId{"int-001"}, core::ContactId{"contact-001"},
                                  core::OpportunityId{"opp-001"}, domain::InteractionState::kDraft};

  repo.upsert(interaction);

  auto retrieved = repo.get(core::InteractionId{"int-001"});
  REQUIRE(retrieved.has_value());
  CHECK(retrieved->interaction_id.value == "int-001");
  CHECK(retrieved->contact_id.value == "contact-001");
  CHECK(retrieved->state == domain::InteractionState::kDraft);
}

TEST_CASE("InMemoryInteractionRepository::list_by_opportunity filters correctly",
          "[storage][repository]") {
  storage::InMemoryInteractionRepository repo;
  repo.upsert({core::InteractionId{"int-001"}, core::ContactId{"contact-001"},
               core::OpportunityId{"opp-001"}, domain::InteractionState::kDraft});
  repo.upsert({core::InteractionId{"int-002"}, core::ContactId{"contact-002"},
               core::OpportunityId{"opp-002"}, domain::InteractionState::kDraft});
  repo.upsert({core::InteractionId{"int-003"}, core::ContactId{"contact-003"},
               core::OpportunityId{"opp-001"}, domain::InteractionState::kReady});

  auto opp1_interactions = repo.list_by_opportunity(core::OpportunityId{"opp-001"});
  REQUIRE(opp1_interactions.size() == 2);
  CHECK(opp1_interactions[0].interaction_id.value == "int-001");
  CHECK(opp1_interactions[1].interaction_id.value == "int-003");
}

TEST_CASE("InMemoryInteractionRepository::list_all returns interactions in deterministic order",
          "[storage][repository]") {
  storage::InMemoryInteractionRepository repo;
  // Insert out of order
  repo.upsert({core::InteractionId{"int-003"}, core::ContactId{"c"}, core::OpportunityId{"o"},
               domain::InteractionState::kDraft});
  repo.upsert({core::InteractionId{"int-001"}, core::ContactId{"a"}, core::OpportunityId{"o"},
               domain::InteractionState::kDraft});
  repo.upsert({core::InteractionId{"int-002"}, core::ContactId{"b"}, core::OpportunityId{"o"},
               domain::InteractionState::kDraft});

  auto all_interactions = repo.list_all();
  REQUIRE(all_interactions.size() == 3);
  // Should be sorted by InteractionId (lexicographic)
  CHECK(all_interactions[0].interaction_id.value == "int-001");
  CHECK(all_interactions[1].interaction_id.value == "int-002");
  CHECK(all_interactions[2].interaction_id.value == "int-003");
}
