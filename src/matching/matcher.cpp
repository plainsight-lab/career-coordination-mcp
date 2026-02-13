#include "ccmcp/matching/matcher.h"

namespace ccmcp::matching {

Matcher::Matcher(const ScoreWeights weights) : weights_(weights) {}

domain::MatchReport Matcher::evaluate(
    const domain::Opportunity& opportunity,
    const std::vector<domain::ExperienceAtom>& atoms) const {
  domain::MatchReport report{};
  report.opportunity_id = opportunity.opportunity_id;
  report.strategy = "deterministic_stub";

  // Deterministic placeholder: select verified atoms only, zero weighted score for now.
  for (const auto& atom : atoms) {
    if (atom.verify()) {
      report.matched_atoms.push_back(atom.atom_id);
    }
  }

  report.breakdown.lexical = 0.0;
  report.breakdown.semantic = 0.0;
  report.breakdown.bonus = 0.0;
  report.breakdown.final_score =
      (weights_.lexical * report.breakdown.lexical) +
      (weights_.semantic * report.breakdown.semantic) +
      (weights_.bonus * report.breakdown.bonus);

  return report;
}

}  // namespace ccmcp::matching
