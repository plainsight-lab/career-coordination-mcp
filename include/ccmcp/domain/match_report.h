#pragma once

#include <string>
#include <vector>

#include "ccmcp/core/ids.h"

namespace ccmcp::domain {

struct ScoreBreakdown {
  double lexical{0.0};
  double semantic{0.0};
  double bonus{0.0};
  double final_score{0.0};
};

struct MatchReport {
  core::OpportunityId opportunity_id;
  std::vector<core::AtomId> matched_atoms;
  std::vector<std::string> missing_requirements;
  ScoreBreakdown breakdown;
  std::string strategy;
};

}  // namespace ccmcp::domain
