#include "ccmcp/storage/inmemory_atom_repository.h"

#include <catch2/catch_test_macros.hpp>

using namespace ccmcp;

TEST_CASE("InMemoryAtomRepository::upsert stores atom", "[storage][repository]") {
  storage::InMemoryAtomRepository repo;
  domain::ExperienceAtom atom{core::AtomId{"atom-001"},
                              "cpp",
                              "Modern C++",
                              "Built C++20 systems",
                              {"cpp20", "systems"},
                              true,
                              {}};

  repo.upsert(atom);

  auto retrieved = repo.get(core::AtomId{"atom-001"});
  REQUIRE(retrieved.has_value());
  CHECK(retrieved->atom_id.value == "atom-001");
  CHECK(retrieved->domain == "cpp");
  CHECK(retrieved->verified == true);
}

TEST_CASE("InMemoryAtomRepository::upsert replaces existing atom", "[storage][repository]") {
  storage::InMemoryAtomRepository repo;
  domain::ExperienceAtom atom1{
      core::AtomId{"atom-001"}, "cpp", "Title 1", "Claim 1", {"tag1"}, false, {}};
  domain::ExperienceAtom atom2{
      core::AtomId{"atom-001"}, "rust", "Title 2", "Claim 2", {"tag2"}, true, {}};

  repo.upsert(atom1);
  repo.upsert(atom2);

  auto retrieved = repo.get(core::AtomId{"atom-001"});
  REQUIRE(retrieved.has_value());
  CHECK(retrieved->domain == "rust");
  CHECK(retrieved->title == "Title 2");
  CHECK(retrieved->verified == true);
}

TEST_CASE("InMemoryAtomRepository::get returns nullopt for missing atom", "[storage][repository]") {
  storage::InMemoryAtomRepository repo;

  auto result = repo.get(core::AtomId{"nonexistent"});
  CHECK_FALSE(result.has_value());
}

TEST_CASE("InMemoryAtomRepository::list_verified returns only verified atoms",
          "[storage][repository]") {
  storage::InMemoryAtomRepository repo;
  repo.upsert({core::AtomId{"atom-001"}, "cpp", "A", "Claim", {}, true, {}});
  repo.upsert({core::AtomId{"atom-002"}, "rust", "B", "Claim", {}, false, {}});
  repo.upsert({core::AtomId{"atom-003"}, "go", "C", "Claim", {}, true, {}});

  auto verified = repo.list_verified();
  REQUIRE(verified.size() == 2);
  CHECK(verified[0].atom_id.value == "atom-001");
  CHECK(verified[1].atom_id.value == "atom-003");
}

TEST_CASE("InMemoryAtomRepository::list_all returns atoms in deterministic order",
          "[storage][repository]") {
  storage::InMemoryAtomRepository repo;
  // Insert out of order
  repo.upsert({core::AtomId{"atom-003"}, "go", "C", "Claim", {}, true, {}});
  repo.upsert({core::AtomId{"atom-001"}, "cpp", "A", "Claim", {}, true, {}});
  repo.upsert({core::AtomId{"atom-002"}, "rust", "B", "Claim", {}, false, {}});

  auto all_atoms = repo.list_all();
  REQUIRE(all_atoms.size() == 3);
  // Should be sorted by AtomId (lexicographic)
  CHECK(all_atoms[0].atom_id.value == "atom-001");
  CHECK(all_atoms[1].atom_id.value == "atom-002");
  CHECK(all_atoms[2].atom_id.value == "atom-003");
}

TEST_CASE("InMemoryAtomRepository::list_verified maintains deterministic order",
          "[storage][repository]") {
  storage::InMemoryAtomRepository repo;
  // Insert out of order
  repo.upsert({core::AtomId{"atom-003"}, "go", "C", "Claim", {}, true, {}});
  repo.upsert({core::AtomId{"atom-001"}, "cpp", "A", "Claim", {}, true, {}});
  repo.upsert({core::AtomId{"atom-002"}, "rust", "B", "Claim", {}, false, {}});

  auto verified = repo.list_verified();
  REQUIRE(verified.size() == 2);
  // Should be sorted by AtomId (lexicographic)
  CHECK(verified[0].atom_id.value == "atom-001");
  CHECK(verified[1].atom_id.value == "atom-003");
}
