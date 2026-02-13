#pragma once

#include <vector>

#include "ccmcp/domain/experience_atom.h"
#include "ccmcp/domain/match_report.h"
#include "ccmcp/domain/opportunity.h"
#include "ccmcp/matching/scorer.h"

namespace ccmcp::matching {

// Matcher is a class (not struct) per C++ Core Guidelines C.2:
// It encapsulates matching logic with private configuration (weights_).
// The invariant is that weights must remain constant after construction for determinism.
class Matcher {
 public:
  explicit Matcher(ScoreWeights weights = ScoreWeights{});

  // Con.2: evaluate() is const - matching is deterministic and doesn't modify matcher state.
  // This enables thread-safe, concurrent evaluations with a single Matcher instance.
  [[nodiscard]] domain::MatchReport evaluate(
      const domain::Opportunity& opportunity,
      const std::vector<domain::ExperienceAtom>& atoms) const;

 private:
  ScoreWeights weights_;
};

}  // namespace ccmcp::matching
