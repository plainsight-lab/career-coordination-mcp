#include "ccmcp/core/id_generator.h"
#include "ccmcp/core/ids.h"
#include "ccmcp/domain/experience_atom.h"
#include "ccmcp/domain/opportunity.h"
#include "ccmcp/domain/requirement.h"
#include "ccmcp/matching/matcher.h"

#include <catch2/catch_test_macros.hpp>

using namespace ccmcp;

TEST_CASE("Matcher tie-breaks by atom_id", "[matching][tie-break]") {
  SECTION("atoms with identical scores use lexicographic atom_id ordering") {
    core::DeterministicIdGenerator gen;

    domain::Opportunity opp;
    opp.opportunity_id = core::new_opportunity_id(gen);
    opp.company = "TestCo";
    opp.role_title = "Engineer";

    domain::Requirement req;
    req.text = "Python experience";
    opp.requirements = {req};

    // Two atoms with identical overlap (both have "python" token)
    std::vector<domain::ExperienceAtom> atoms;

    domain::ExperienceAtom atom_b;
    atom_b.atom_id.value = "atom-zzz";  // Lexicographically larger
    atom_b.domain = "backend";
    atom_b.claim = "Python development";
    atom_b.tags = {"python"};
    atom_b.verified = true;
    atoms.push_back(atom_b);

    domain::ExperienceAtom atom_a;
    atom_a.atom_id.value = "atom-aaa";  // Lexicographically smaller (should win)
    atom_a.domain = "data";
    atom_a.claim = "Python analytics";
    atom_a.tags = {"python"};
    atom_a.verified = true;
    atoms.push_back(atom_a);

    // Execute matcher
    matching::Matcher matcher;
    auto report = matcher.evaluate(opp, atoms);

    // Verify: atom-aaa wins (lexicographically smaller)
    REQUIRE(report.requirement_matches.size() == 1);
    REQUIRE(report.requirement_matches[0].matched == true);
    REQUIRE(report.requirement_matches[0].contributing_atom_id.has_value());
    REQUIRE(report.requirement_matches[0].contributing_atom_id->value == "atom-aaa");
  }

  SECTION("tie-break is deterministic across runs") {
    core::DeterministicIdGenerator gen;

    domain::Opportunity opp;
    opp.opportunity_id = core::new_opportunity_id(gen);
    opp.company = "TestCo";
    opp.role_title = "Engineer";

    domain::Requirement req;
    req.text = "Go microservices";
    opp.requirements = {req};

    std::vector<domain::ExperienceAtom> atoms;

    domain::ExperienceAtom atom1;
    atom1.atom_id.value = "atom-003";
    atom1.claim = "Go microservices development";
    atom1.tags = {"go", "microservices"};
    atom1.verified = true;
    atoms.push_back(atom1);

    domain::ExperienceAtom atom2;
    atom2.atom_id.value = "atom-002";  // Should win
    atom2.claim = "Built Go microservices";
    atom2.tags = {"go", "microservices"};
    atom2.verified = true;
    atoms.push_back(atom2);

    domain::ExperienceAtom atom3;
    atom3.atom_id.value = "atom-001";  // Smallest, should win if all equal
    atom3.claim = "Go microservices architecture";
    atom3.tags = {"go", "microservices"};
    atom3.verified = true;
    atoms.push_back(atom3);

    // All three atoms have identical overlap
    matching::Matcher matcher;
    auto report1 = matcher.evaluate(opp, atoms);
    auto report2 = matcher.evaluate(opp, atoms);

    // Verify: atom-001 wins consistently
    REQUIRE(report1.requirement_matches[0].contributing_atom_id->value == "atom-001");
    REQUIRE(report2.requirement_matches[0].contributing_atom_id->value == "atom-001");
  }
}
