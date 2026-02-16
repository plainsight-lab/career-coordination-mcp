#include "ccmcp/core/id_generator.h"
#include "ccmcp/core/ids.h"
#include "ccmcp/domain/experience_atom.h"
#include "ccmcp/domain/opportunity.h"
#include "ccmcp/domain/requirement.h"
#include "ccmcp/matching/matcher.h"

#include <catch2/catch_test_macros.hpp>

using namespace ccmcp;

TEST_CASE("Matcher produces lexical overlap scores", "[matching][basic]") {
  SECTION("requirement matches atom with overlapping tokens") {
    core::DeterministicIdGenerator gen;

    // Setup: Opportunity with one requirement
    domain::Opportunity opp;
    opp.opportunity_id = core::new_opportunity_id(gen);
    opp.company = "TestCo";
    opp.role_title = "Architect";

    domain::Requirement req;
    req.text = "C++ governance architecture";
    req.tags = {};
    req.required = true;
    opp.requirements = {req};

    // Setup: Two atoms (one relevant, one not)
    std::vector<domain::ExperienceAtom> atoms;

    domain::ExperienceAtom atom_a;
    atom_a.atom_id = core::new_atom_id(gen);
    atom_a.domain = "architecture";
    atom_a.title = "Architecture Lead";
    atom_a.claim = "Led C++ architecture decisions for governance systems";
    atom_a.tags = {"cpp", "architecture", "governance"};
    atom_a.verified = true;
    atoms.push_back(atom_a);

    domain::ExperienceAtom atom_b;
    atom_b.atom_id = core::new_atom_id(gen);
    atom_b.domain = "backend";
    atom_b.title = "Backend Developer";
    atom_b.claim = "Built Python microservices";
    atom_b.tags = {"python", "backend"};
    atom_b.verified = true;
    atoms.push_back(atom_b);

    // Execute matcher
    matching::Matcher matcher;
    auto report = matcher.evaluate(opp, atoms);

    // Verify: Requirement matched to atom_a
    REQUIRE(report.requirement_matches.size() == 1);
    REQUIRE(report.requirement_matches[0].matched == true);
    REQUIRE(report.requirement_matches[0].best_score > 0.0);
    REQUIRE(report.requirement_matches[0].contributing_atom_id.has_value());
    REQUIRE(report.requirement_matches[0].contributing_atom_id->value == atom_a.atom_id.value);

    // Verify: Evidence tokens include overlap
    // Requirement tokens: "c", "governance", "architecture" (after tokenization)
    // Atom_a tokens include: "architecture", "governance", "c" (from "cpp" and claim)
    // Expected overlap includes at least: "architecture", "governance"
    REQUIRE(!report.requirement_matches[0].evidence_tokens.empty());

    // Verify: Overall score > 0
    REQUIRE(report.overall_score > 0.0);

    // Verify: Missing requirements is empty
    REQUIRE(report.missing_requirements.empty());
  }

  SECTION("unverified atoms are not considered") {
    core::DeterministicIdGenerator gen;

    domain::Opportunity opp;
    opp.opportunity_id = core::new_opportunity_id(gen);
    opp.company = "TestCo";
    opp.role_title = "Architect";

    domain::Requirement req;
    req.text = "Python experience";
    opp.requirements = {req};

    // Atom matches requirement but is not verified
    std::vector<domain::ExperienceAtom> atoms;
    domain::ExperienceAtom atom;
    atom.atom_id = core::new_atom_id(gen);
    atom.domain = "backend";
    atom.claim = "Built Python systems";
    atom.tags = {"python"};
    atom.verified = false;  // Not verified
    atoms.push_back(atom);

    matching::Matcher matcher;
    auto report = matcher.evaluate(opp, atoms);

    // Requirement should be unmatched
    REQUIRE(report.requirement_matches.size() == 1);
    REQUIRE(report.requirement_matches[0].matched == false);
    REQUIRE(report.requirement_matches[0].best_score == 0.0);
    REQUIRE(!report.requirement_matches[0].contributing_atom_id.has_value());
    REQUIRE(report.missing_requirements.size() == 1);
  }
}
