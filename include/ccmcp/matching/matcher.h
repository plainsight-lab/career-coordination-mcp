#pragma once

#include <vector>

#include "ccmcp/domain/experience_atom.h"
#include "ccmcp/domain/match_report.h"
#include "ccmcp/domain/opportunity.h"
#include "ccmcp/matching/scorer.h"

namespace ccmcp::matching {

class Matcher {
 public:
  explicit Matcher(ScoreWeights weights = ScoreWeights{});

  [[nodiscard]] domain::MatchReport evaluate(
      const domain::Opportunity& opportunity,
      const std::vector<domain::ExperienceAtom>& atoms) const;

 private:
  ScoreWeights weights_;
};

}  // namespace ccmcp::matching
