#include "ccmcp/matching/matcher.h"

#include "ccmcp/core/normalization.h"

#include <algorithm>
#include <map>
#include <set>
#include <string>

namespace ccmcp::matching {

namespace {

// Helper: Build query text from opportunity requirements
std::string build_query_text(const domain::Opportunity& opportunity) {
  std::string query;
  for (const auto& req : opportunity.requirements) {
    if (!query.empty()) {
      query += " ";
    }
    query += req.text;
  }
  return query;
}

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

Matcher::Matcher(const ScoreWeights weights, const MatchingStrategy strategy,
                 const HybridConfig hybrid_config)
    : weights_(weights), strategy_(strategy), hybrid_config_(hybrid_config) {}

std::vector<const domain::ExperienceAtom*> Matcher::select_candidates(
    const domain::Opportunity& opportunity, const std::vector<domain::ExperienceAtom>& atoms,
    const embedding::IEmbeddingProvider* embedding_provider,
    const vector::IEmbeddingIndex* vector_index, domain::RetrievalStats& stats) const {
  // v0.1 mode: All verified atoms are candidates
  if (strategy_ == MatchingStrategy::kDeterministicLexicalV01) {
    std::vector<const domain::ExperienceAtom*> candidates;
    for (const auto& atom : atoms) {
      if (atom.verify()) {
        candidates.push_back(&atom);
      }
    }
    stats.lexical_candidates = candidates.size();
    stats.embedding_candidates = 0;
    stats.merged_candidates = candidates.size();
    return candidates;
  }

  // v0.2 hybrid mode: Lexical top-K + embedding top-K, merged
  std::set<std::string> lexical_atom_ids;
  std::set<std::string> embedding_atom_ids;

  // Stage 1: Lexical candidate selection
  // Pre-compute lexical overlap scores for all verified atoms
  struct ScoredAtom {
    const domain::ExperienceAtom* atom;
    double score;
  };

  std::vector<ScoredAtom> lexical_scored;

  // Tokenize all requirements into combined query
  std::vector<std::string> all_req_tokens;
  for (const auto& req : opportunity.requirements) {
    auto req_tokens = tokenize_field(req.text);
    all_req_tokens.insert(all_req_tokens.end(), req_tokens.begin(), req_tokens.end());
  }

  // Deduplicate and sort query tokens
  std::set<std::string> unique_tokens(all_req_tokens.begin(), all_req_tokens.end());
  std::vector<std::string> query_tokens(unique_tokens.begin(), unique_tokens.end());

  if (query_tokens.empty()) {
    // No query tokens - fallback to all verified atoms
    std::vector<const domain::ExperienceAtom*> candidates;
    for (const auto& atom : atoms) {
      if (atom.verify()) {
        candidates.push_back(&atom);
      }
    }
    stats.lexical_candidates = candidates.size();
    stats.embedding_candidates = 0;
    stats.merged_candidates = candidates.size();
    return candidates;
  }

  // Score all verified atoms by lexical overlap
  for (const auto& atom : atoms) {
    if (!atom.verify()) {
      continue;
    }

    // Tokenize atom
    std::vector<std::string> claim_tokens = tokenize_field(atom.claim);
    std::vector<std::string> title_tokens = tokenize_field(atom.title);
    std::vector<std::string> atom_tokens =
        combine_token_sets(claim_tokens, title_tokens, atom.tags);

    // Compute overlap score
    std::vector<std::string> intersection = extract_intersection(query_tokens, atom_tokens);
    double score =
        static_cast<double>(intersection.size()) / static_cast<double>(query_tokens.size());

    lexical_scored.push_back({&atom, score});
  }

  // Sort by score descending, then atom_id ascending (deterministic tie-break)
  std::sort(lexical_scored.begin(), lexical_scored.end(),
            [](const ScoredAtom& a, const ScoredAtom& b) {
              if (a.score != b.score) {
                return a.score > b.score;
              }
              return a.atom->atom_id.value < b.atom->atom_id.value;
            });

  // Select top K_lex
  size_t k_lex = std::min(hybrid_config_.k_lexical, lexical_scored.size());
  for (size_t i = 0; i < k_lex; ++i) {
    lexical_atom_ids.insert(lexical_scored[i].atom->atom_id.value);
  }

  stats.lexical_candidates = lexical_atom_ids.size();

  // Stage 2: Embedding candidate selection
  if (embedding_provider != nullptr && vector_index != nullptr &&
      embedding_provider->dimension() > 0) {
    // Build query embedding
    std::string query_text = build_query_text(opportunity);
    vector::Vector query_embedding = embedding_provider->embed_text(query_text);

    if (!query_embedding.empty()) {
      // Query vector index for top K_emb
      auto search_results = vector_index->query(query_embedding, hybrid_config_.k_embedding);

      // Extract atom IDs from search results
      for (const auto& result : search_results) {
        embedding_atom_ids.insert(result.key);
      }
    }
  }

  stats.embedding_candidates = embedding_atom_ids.size();

  // Stage 3: Merge candidates (union by atom_id)
  std::set<std::string> merged_atom_ids;
  merged_atom_ids.insert(lexical_atom_ids.begin(), lexical_atom_ids.end());
  merged_atom_ids.insert(embedding_atom_ids.begin(), embedding_atom_ids.end());

  stats.merged_candidates = merged_atom_ids.size();

  // Build atom map for fast lookup
  std::map<std::string, const domain::ExperienceAtom*> atom_map;
  for (const auto& atom : atoms) {
    atom_map[atom.atom_id.value] = &atom;
  }

  // Build final candidate list (sorted by atom_id for determinism)
  std::vector<const domain::ExperienceAtom*> candidates;
  for (const auto& atom_id : merged_atom_ids) {
    auto it = atom_map.find(atom_id);
    if (it != atom_map.end()) {
      candidates.push_back(it->second);
    }
  }

  return candidates;
}

domain::MatchReport Matcher::evaluate(const domain::Opportunity& opportunity,
                                      const std::vector<domain::ExperienceAtom>& atoms,
                                      const embedding::IEmbeddingProvider* embedding_provider,
                                      const vector::IEmbeddingIndex* vector_index) const {
  domain::MatchReport report{};
  report.opportunity_id = opportunity.opportunity_id;

  // Set strategy string based on mode
  if (strategy_ == MatchingStrategy::kDeterministicLexicalV01) {
    report.strategy = "deterministic_lexical_v0.1";
  } else {
    report.strategy = "hybrid_lexical_embedding_v0.2";
  }

  // Select candidate atoms based on strategy
  auto candidates = select_candidates(opportunity, atoms, embedding_provider, vector_index,
                                      report.retrieval_stats);

  // Pre-compute atom token sets for candidates only (avoid redundant tokenization)
  std::map<std::string, std::vector<std::string>> candidate_token_sets;

  for (const auto* atom_ptr : candidates) {
    std::vector<std::string> claim_tokens = tokenize_field(atom_ptr->claim);
    std::vector<std::string> title_tokens = tokenize_field(atom_ptr->title);
    std::vector<std::string> atom_tokens =
        combine_token_sets(claim_tokens, title_tokens, atom_ptr->tags);
    candidate_token_sets[atom_ptr->atom_id.value] = std::move(atom_tokens);
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

    // Find best matching atom from candidates using pre-computed token sets
    double best_score = 0.0;
    std::optional<core::AtomId> best_atom_id;
    std::vector<std::string> best_evidence;

    for (const auto* atom_ptr : candidates) {
      const auto& atom = *atom_ptr;
      const auto& atom_tokens = candidate_token_sets[atom.atom_id.value];

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
