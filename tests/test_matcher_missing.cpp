#include "ccmcp/core/id_generator.h"
#include "ccmcp/core/ids.h"
#include "ccmcp/domain/experience_atom.h"
#include "ccmcp/domain/opportunity.h"
#include "ccmcp/domain/requirement.h"
#include "ccmcp/matching/matcher.h"

#include <catch2/catch_test_macros.hpp>

using namespace ccmcp;

TEST_CASE("Matcher handles missing requirements", "[matching][missing]") {
  SECTION("requirement with no overlap is marked unmatched") {
    core::DeterministicIdGenerator gen;

    domain::Opportunity opp;
    opp.opportunity_id = core::new_opportunity_id(gen);
    opp.company = "TestCo";
    opp.role_title = "Engineer";

    domain::Requirement req;
    req.text = "Rust systems programming";
    opp.requirements = {req};

    // Atom has no overlap with requirement
    std::vector<domain::ExperienceAtom> atoms;
    domain::ExperienceAtom atom;
    atom.atom_id = core::new_atom_id(gen);
    atom.claim = "Built Python web applications";
    atom.tags = {"python", "web"};
    atom.verified = true;
    atoms.push_back(atom);

    matching::Matcher matcher;
    auto report = matcher.evaluate(opp, atoms);

    // Verify: Requirement is unmatched
    REQUIRE(report.requirement_matches.size() == 1);
    REQUIRE(report.requirement_matches[0].matched == false);
    REQUIRE(report.requirement_matches[0].best_score == 0.0);
    REQUIRE(!report.requirement_matches[0].contributing_atom_id.has_value());
    REQUIRE(report.requirement_matches[0].evidence_tokens.empty());

    // Verify: Appears in missing_requirements
    REQUIRE(report.missing_requirements.size() == 1);
    REQUIRE(report.missing_requirements[0] == req.text);
  }

  SECTION("partial match does not appear in missing_requirements") {
    core::DeterministicIdGenerator gen;

    domain::Opportunity opp;
    opp.opportunity_id = core::new_opportunity_id(gen);
    opp.company = "TestCo";
    opp.role_title = "Engineer";

    domain::Requirement req1;
    req1.text = "Python experience";  // Will match
    opp.requirements.push_back(req1);

    domain::Requirement req2;
    req2.text = "Rust experience";  // Will NOT match
    opp.requirements.push_back(req2);

    std::vector<domain::ExperienceAtom> atoms;
    domain::ExperienceAtom atom;
    atom.atom_id = core::new_atom_id(gen);
    atom.claim = "Built Python systems";
    atom.tags = {"python"};
    atom.verified = true;
    atoms.push_back(atom);

    matching::Matcher matcher;
    auto report = matcher.evaluate(opp, atoms);

    // Verify: First requirement matched, second missing
    REQUIRE(report.requirement_matches.size() == 2);
    REQUIRE(report.requirement_matches[0].matched == true);
    REQUIRE(report.requirement_matches[1].matched == false);

    // Verify: Only second requirement in missing list
    REQUIRE(report.missing_requirements.size() == 1);
    REQUIRE(report.missing_requirements[0] == req2.text);

    // Verify: Overall score reflects partial match
    REQUIRE(report.overall_score > 0.0);
    REQUIRE(report.overall_score < 1.0);
  }

  SECTION("zero requirements produces zero overall score") {
    core::DeterministicIdGenerator gen;

    domain::Opportunity opp;
    opp.opportunity_id = core::new_opportunity_id(gen);
    opp.company = "TestCo";
    opp.role_title = "Engineer";
    opp.requirements = {};  // No requirements

    std::vector<domain::ExperienceAtom> atoms;
    domain::ExperienceAtom atom;
    atom.atom_id = core::new_atom_id(gen);
    atom.claim = "Some experience";
    atom.verified = true;
    atoms.push_back(atom);

    matching::Matcher matcher;
    auto report = matcher.evaluate(opp, atoms);

    // Verify: Zero score, no matches, no missing
    REQUIRE(report.overall_score == 0.0);
    REQUIRE(report.requirement_matches.empty());
    REQUIRE(report.missing_requirements.empty());
  }

  SECTION("empty requirement text is marked unmatched") {
    core::DeterministicIdGenerator gen;

    domain::Opportunity opp;
    opp.opportunity_id = core::new_opportunity_id(gen);
    opp.company = "TestCo";
    opp.role_title = "Engineer";

    domain::Requirement req;
    req.text = "";  // Empty text produces no tokens
    opp.requirements = {req};

    std::vector<domain::ExperienceAtom> atoms;
    domain::ExperienceAtom atom;
    atom.atom_id = core::new_atom_id(gen);
    atom.claim = "Experience";
    atom.verified = true;
    atoms.push_back(atom);

    matching::Matcher matcher;
    auto report = matcher.evaluate(opp, atoms);

    // Verify: Empty requirement is unmatched
    REQUIRE(report.requirement_matches.size() == 1);
    REQUIRE(report.requirement_matches[0].matched == false);
    REQUIRE(report.requirement_matches[0].best_score == 0.0);
    REQUIRE(report.missing_requirements.size() == 1);
  }
}
