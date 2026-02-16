#include "ccmcp/core/id_generator.h"
#include "ccmcp/core/ids.h"
#include "ccmcp/domain/experience_atom.h"
#include "ccmcp/domain/opportunity.h"
#include "ccmcp/domain/requirement.h"
#include "ccmcp/matching/matcher.h"

#include <catch2/catch_test_macros.hpp>

#include <sstream>

using namespace ccmcp;

TEST_CASE("Matcher produces deterministic results", "[matching][determinism]") {
  SECTION("identical input produces identical output") {
    core::DeterministicIdGenerator gen;

    // Setup identical opportunity
    domain::Opportunity opp;
    opp.opportunity_id = core::new_opportunity_id(gen);
    opp.company = "TestCo";
    opp.role_title = "Engineer";

    domain::Requirement req1;
    req1.text = "Python and Docker experience";
    opp.requirements = {req1};

    domain::Requirement req2;
    req2.text = "AWS cloud infrastructure";
    opp.requirements.push_back(req2);

    // Setup identical atoms
    std::vector<domain::ExperienceAtom> atoms;

    domain::ExperienceAtom atom1;
    atom1.atom_id.value = "atom-001";  // Fixed ID for determinism
    atom1.domain = "backend";
    atom1.claim = "Built Python systems with Docker";
    atom1.tags = {"docker", "python"};
    atom1.verified = true;
    atoms.push_back(atom1);

    domain::ExperienceAtom atom2;
    atom2.atom_id.value = "atom-002";
    atom2.domain = "cloud";
    atom2.claim = "Managed AWS infrastructure";
    atom2.tags = {"aws", "cloud"};
    atom2.verified = true;
    atoms.push_back(atom2);

    // Run matcher twice
    matching::Matcher matcher;
    auto report1 = matcher.evaluate(opp, atoms);
    auto report2 = matcher.evaluate(opp, atoms);

    // Verify identical results
    REQUIRE(report1.overall_score == report2.overall_score);
    REQUIRE(report1.requirement_matches.size() == report2.requirement_matches.size());

    for (std::size_t i = 0; i < report1.requirement_matches.size(); ++i) {
      const auto& rm1 = report1.requirement_matches[i];
      const auto& rm2 = report2.requirement_matches[i];

      REQUIRE(rm1.requirement_text == rm2.requirement_text);
      REQUIRE(rm1.matched == rm2.matched);
      REQUIRE(rm1.best_score == rm2.best_score);
      REQUIRE(rm1.contributing_atom_id.has_value() == rm2.contributing_atom_id.has_value());
      if (rm1.contributing_atom_id.has_value()) {
        REQUIRE(rm1.contributing_atom_id->value == rm2.contributing_atom_id->value);
      }
      REQUIRE(rm1.evidence_tokens == rm2.evidence_tokens);
    }

    REQUIRE(report1.missing_requirements == report2.missing_requirements);
    REQUIRE(report1.matched_atoms.size() == report2.matched_atoms.size());
  }

  SECTION("evidence tokens are sorted and stable") {
    core::DeterministicIdGenerator gen;

    domain::Opportunity opp;
    opp.opportunity_id = core::new_opportunity_id(gen);
    opp.company = "TestCo";
    opp.role_title = "Engineer";

    domain::Requirement req;
    req.text = "kubernetes docker aws terraform";
    opp.requirements = {req};

    std::vector<domain::ExperienceAtom> atoms;
    domain::ExperienceAtom atom;
    atom.atom_id = core::new_atom_id(gen);
    atom.claim = "terraform aws kubernetes infrastructure";
    atom.tags = {"aws", "kubernetes", "terraform"};
    atom.verified = true;
    atoms.push_back(atom);

    matching::Matcher matcher;
    auto report = matcher.evaluate(opp, atoms);

    REQUIRE(report.requirement_matches.size() == 1);
    REQUIRE(report.requirement_matches[0].matched == true);

    // Evidence tokens must be sorted
    const auto& evidence = report.requirement_matches[0].evidence_tokens;
    REQUIRE(!evidence.empty());
    for (std::size_t i = 1; i < evidence.size(); ++i) {
      REQUIRE(evidence[i - 1] < evidence[i]);
    }
  }
}
