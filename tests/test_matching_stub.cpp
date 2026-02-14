#include "ccmcp/core/ids.h"
#include "ccmcp/domain/experience_atom.h"
#include "ccmcp/domain/opportunity.h"
#include "ccmcp/matching/matcher.h"

#include <catch2/catch_test_macros.hpp>

#include <vector>

TEST_CASE("Matcher stub only returns verified atoms", "[matching]") {
  ccmcp::domain::Opportunity opportunity{};
  opportunity.opportunity_id = ccmcp::core::new_opportunity_id();

  std::vector<ccmcp::domain::ExperienceAtom> atoms{
      {ccmcp::core::new_atom_id(), "domain", "t1", "c1", {}, true},
      {ccmcp::core::new_atom_id(), "domain", "t2", "c2", {}, false},
  };

  ccmcp::matching::Matcher matcher;
  const auto report = matcher.evaluate(opportunity, atoms);

  REQUIRE(report.matched_atoms.size() == 1);
  REQUIRE(report.breakdown.final_score == 0.0);
}
