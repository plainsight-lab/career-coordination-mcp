#include "ccmcp/core/ids.h"
#include "ccmcp/domain/experience_atom.h"
#include "ccmcp/domain/opportunity.h"
#include "ccmcp/domain/requirement.h"
#include "ccmcp/matching/matcher.h"

#include <catch2/catch_test_macros.hpp>

#include <vector>

TEST_CASE("Matcher only considers verified atoms", "[matching]") {
  ccmcp::domain::Opportunity opportunity{};
  opportunity.opportunity_id = ccmcp::core::new_opportunity_id();
  opportunity.company = "TestCo";
  opportunity.role_title = "Engineer";

  // Add a requirement to match against
  ccmcp::domain::Requirement req;
  req.text = "domain experience";
  opportunity.requirements = {req};

  std::vector<ccmcp::domain::ExperienceAtom> atoms;

  // Verified atom that matches
  ccmcp::domain::ExperienceAtom verified_atom;
  verified_atom.atom_id = ccmcp::core::new_atom_id();
  verified_atom.domain = "domain";
  verified_atom.title = "Domain Expert";
  verified_atom.claim = "Domain experience";
  verified_atom.tags = {};
  verified_atom.verified = true;
  atoms.push_back(verified_atom);

  // Unverified atom (even though it would match, should be ignored)
  ccmcp::domain::ExperienceAtom unverified_atom;
  unverified_atom.atom_id = ccmcp::core::new_atom_id();
  unverified_atom.domain = "domain";
  unverified_atom.title = "Domain Expert";
  unverified_atom.claim = "Domain experience";
  unverified_atom.tags = {};
  unverified_atom.verified = false;
  atoms.push_back(unverified_atom);

  ccmcp::matching::Matcher matcher;
  const auto report = matcher.evaluate(opportunity, atoms);

  // Only verified atom should contribute to match
  REQUIRE(report.matched_atoms.size() == 1);
  REQUIRE(report.matched_atoms[0].value == verified_atom.atom_id.value);

  // Requirement should be matched
  REQUIRE(report.requirement_matches.size() == 1);
  REQUIRE(report.requirement_matches[0].matched == true);
  REQUIRE(report.requirement_matches[0].contributing_atom_id.has_value());
  REQUIRE(report.requirement_matches[0].contributing_atom_id->value == verified_atom.atom_id.value);
}
