#include "ccmcp/matching/matcher.h"

#include "ccmcp/core/normalization.h"

#include <algorithm>
#include <set>
#include <string>

namespace ccmcp::matching {

namespace {

// Extract intersection tokens (sorted) using std::set_intersection.
// Both inputs must be sorted and deduplicated.
std::vector<std::string> extract_intersection(const std::vector<std::string>& a,
                                              const std::vector<std::string>& b) {
  std::vector<std::string> result;
  std::set_intersection(a.begin(), a.end(), b.begin(), b.end(), std::back_inserter(result));
  return result;
}

// Tokenize and normalize a text field into sorted, deduplicated tokens.
std::vector<std::string> tokenize_field(const std::string& text) {
  auto tokens = core::tokenize_ascii(text);
  // tokenize_ascii already lowercases, but we ensure deduplication and sorting
  std::set<std::string> unique_tokens(tokens.begin(), tokens.end());
  return std::vector<std::string>(unique_tokens.begin(), unique_tokens.end());
}

// Combine multiple token sets into one sorted, deduplicated set.
std::vector<std::string> combine_token_sets(const std::vector<std::string>& a,
                                            const std::vector<std::string>& b,
                                            const std::vector<std::string>& c) {
  std::set<std::string> combined;
  combined.insert(a.begin(), a.end());
  combined.insert(b.begin(), b.end());
  combined.insert(c.begin(), c.end());
  return std::vector<std::string>(combined.begin(), combined.end());
}

}  // namespace

Matcher::Matcher(const ScoreWeights weights) : weights_(weights) {}

domain::MatchReport Matcher::evaluate(const domain::Opportunity& opportunity,
                                      const std::vector<domain::ExperienceAtom>& atoms) const {
  domain::MatchReport report{};
  report.opportunity_id = opportunity.opportunity_id;
  report.strategy = "deterministic_lexical_v0.1";

  // Pre-compute atom token sets once (avoid O(R × A) redundant tokenization)
  // This moves tokenization from O(R × A × T) to O(A × T) where T = tokenization cost
  std::vector<std::vector<std::string>> atom_token_sets;
  atom_token_sets.reserve(atoms.size());

  for (const auto& atom : atoms) {
    if (atom.verify()) {
      std::vector<std::string> claim_tokens = tokenize_field(atom.claim);
      std::vector<std::string> title_tokens = tokenize_field(atom.title);
      std::vector<std::string> atom_tokens =
          combine_token_sets(claim_tokens, title_tokens, atom.tags);
      atom_token_sets.push_back(std::move(atom_tokens));
    } else {
      // Unverified atoms get empty token set (will be skipped during matching)
      atom_token_sets.emplace_back();
    }
  }

  // Process each requirement in order (preserving input order)
  double total_score = 0.0;
  std::set<std::string> matched_atom_ids;  // Track unique matched atoms

  for (const auto& req : opportunity.requirements) {
    domain::RequirementMatch req_match;
    req_match.requirement_text = req.text;

    // Tokenize requirement text
    std::vector<std::string> req_tokens = tokenize_field(req.text);

    // If requirement has no tokens, mark as unmatched
    if (req_tokens.empty()) {
      req_match.matched = false;
      req_match.best_score = 0.0;
      report.requirement_matches.push_back(req_match);
      report.missing_requirements.push_back(req.text);
      continue;
    }

    // Find best matching atom using pre-computed token sets
    double best_score = 0.0;
    std::optional<core::AtomId> best_atom_id;
    std::vector<std::string> best_evidence;

    for (std::size_t i = 0; i < atoms.size(); ++i) {
      const auto& atom = atoms[i];
      const auto& atom_tokens = atom_token_sets[i];

      // Skip unverified atoms (empty token set)
      if (atom_tokens.empty()) {
        continue;
      }

      // Compute overlap: |R ∩ A| / |R| (ES.1: use std::set_intersection)
      std::vector<std::string> intersection = extract_intersection(req_tokens, atom_tokens);
      double score =
          static_cast<double>(intersection.size()) / static_cast<double>(req_tokens.size());

      // Update best match (tie-break by lexicographically smaller atom_id)
      if (score > best_score ||
          (score == best_score &&
           (!best_atom_id.has_value() || atom.atom_id.value < best_atom_id->value))) {
        best_score = score;
        best_atom_id = atom.atom_id;
        best_evidence = std::move(intersection);  // Reuse already-computed intersection
      }
    }

    // Record match result
    req_match.best_score = best_score;
    if (best_score > 0.0 && best_atom_id.has_value()) {
      req_match.matched = true;
      req_match.contributing_atom_id = best_atom_id;
      req_match.evidence_tokens = best_evidence;
      matched_atom_ids.insert(best_atom_id->value);
    } else {
      req_match.matched = false;
      report.missing_requirements.push_back(req.text);
    }

    report.requirement_matches.push_back(req_match);
    total_score += best_score;
  }

  // Calculate overall score (average of per-requirement scores)
  if (!opportunity.requirements.empty()) {
    report.overall_score = total_score / static_cast<double>(opportunity.requirements.size());
  } else {
    report.overall_score = 0.0;
  }

  // Populate legacy matched_atoms field (sorted for determinism)
  for (const auto& atom_id_str : matched_atom_ids) {
    core::AtomId atom_id;
    atom_id.value = atom_id_str;
    report.matched_atoms.push_back(atom_id);
  }

  // Populate breakdown (v0.1: only lexical scoring)
  report.breakdown.lexical = report.overall_score;
  report.breakdown.semantic = 0.0;
  report.breakdown.bonus = 0.0;
  report.breakdown.final_score = weights_.lexical * report.breakdown.lexical;

  return report;
}

}  // namespace ccmcp::matching
