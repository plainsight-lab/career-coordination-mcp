#include <iostream>
#include <vector>

#include <nlohmann/json.hpp>

#include "ccmcp/core/ids.h"
#include "ccmcp/domain/experience_atom.h"
#include "ccmcp/domain/opportunity.h"
#include "ccmcp/domain/requirement.h"
#include "ccmcp/matching/matcher.h"

int main() {
  std::cout << "career-coordination-mcp v0.1\n";

  ccmcp::domain::Opportunity opportunity{};
  opportunity.opportunity_id = ccmcp::core::new_opportunity_id();
  opportunity.company = "ExampleCo";
  opportunity.role_title = "Principal Architect";
  opportunity.source = "manual";
  opportunity.requirements = {
      {"req-1", ccmcp::domain::RequirementType::kSkill, "C++20"},
      {"req-2", ccmcp::domain::RequirementType::kDomain, "Architecture"},
  };

  std::vector<ccmcp::domain::ExperienceAtom> atoms;
  atoms.push_back({ccmcp::core::new_atom_id(), "architecture", "Architecture Leadership",
                   "Led architecture decisions", {"architecture", "governance"}, true});
  atoms.push_back({ccmcp::core::new_atom_id(), "cpp", "Modern C++",
                   "Built C++20 systems", {"cpp20", "systems"}, false});

  ccmcp::matching::Matcher matcher;
  const auto report = matcher.evaluate(opportunity, atoms);

  nlohmann::json out;
  out["opportunity_id"] = report.opportunity_id.value;
  out["strategy"] = report.strategy;
  out["scores"] = {
      {"lexical", report.breakdown.lexical},
      {"semantic", report.breakdown.semantic},
      {"bonus", report.breakdown.bonus},
      {"final", report.breakdown.final_score},
  };

  out["matched_atoms"] = nlohmann::json::array();
  for (const auto& atom_id : report.matched_atoms) {
    out["matched_atoms"].push_back(atom_id.value);
  }

  std::cout << out.dump(2) << "\n";
  return 0;
}
