#pragma once

#include "ccmcp/core/ids.h"

#include <optional>
#include <string>
#include <vector>

namespace ccmcp::domain {

struct ScoreBreakdown {
  double lexical{0.0};
  double semantic{0.0};
  double bonus{0.0};
  double final_score{0.0};
};

// RequirementMatch tracks the match result for a single requirement.
// v0.1: Deterministic lexical matching with evidence attribution.
struct RequirementMatch {
  std::string requirement_text;              // Original requirement text
  bool matched{false};                       // Whether requirement was matched
  double best_score{0.0};                    // Best overlap score (0.0 if no match)
  std::optional<core::AtomId> contributing_atom_id;  // Atom that matched (if any)
  std::vector<std::string> evidence_tokens;  // Overlap tokens (sorted)
};

struct MatchReport {
  core::OpportunityId opportunity_id;
  std::string strategy;
  double overall_score{0.0};                      // Average of per-requirement scores
  std::vector<RequirementMatch> requirement_matches;  // Per-requirement details (preserves order)
  std::vector<std::string> missing_requirements;  // Unmatched requirement texts

  // Legacy fields (kept for backward compatibility)
  std::vector<core::AtomId> matched_atoms;  // All atoms that contributed to any match
  ScoreBreakdown breakdown;
};

}  // namespace ccmcp::domain
