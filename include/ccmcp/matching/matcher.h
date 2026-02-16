#pragma once

#include "ccmcp/domain/experience_atom.h"
#include "ccmcp/domain/match_report.h"
#include "ccmcp/domain/opportunity.h"
#include "ccmcp/embedding/embedding_provider.h"
#include "ccmcp/matching/scorer.h"
#include "ccmcp/vector/embedding_index.h"

#include <vector>

namespace ccmcp::matching {

// MatchingStrategy defines retrieval modes for candidate selection.
enum class MatchingStrategy {
  kDeterministicLexicalV01,    // v0.1: Pure lexical overlap (default)
  kHybridLexicalEmbeddingV02,  // v0.2: Lexical + embedding recall expansion
};

// HybridConfig controls hybrid retrieval parameters.
struct HybridConfig {
  size_t k_lexical{25};    // Top K candidates from lexical pre-scoring
  size_t k_embedding{25};  // Top K candidates from embedding similarity
};

// Matcher is a class (not struct) per C++ Core Guidelines C.2:
// It encapsulates matching logic with private configuration (weights_).
// The invariant is that weights must remain constant after construction for determinism.
class Matcher {
 public:
  explicit Matcher(ScoreWeights weights = ScoreWeights{},
                   MatchingStrategy strategy = MatchingStrategy::kDeterministicLexicalV01,
                   HybridConfig hybrid_config = HybridConfig{});

  // Con.2: evaluate() is const - matching is deterministic and doesn't modify matcher state.
  // This enables thread-safe, concurrent evaluations with a single Matcher instance.
  //
  // v0.1 mode: All atoms used for scoring (lexical overlap)
  // v0.2 hybrid mode: Requires embedding_provider and vector_index
  //   - Lexical top-K + embedding top-K merged, then scored
  [[nodiscard]] domain::MatchReport evaluate(
      const domain::Opportunity& opportunity, const std::vector<domain::ExperienceAtom>& atoms,
      const embedding::IEmbeddingProvider* embedding_provider = nullptr,
      const vector::IEmbeddingIndex* vector_index = nullptr) const;

 private:
  ScoreWeights weights_;
  MatchingStrategy strategy_;
  HybridConfig hybrid_config_;

  // Helper: Select candidate atoms for scoring
  [[nodiscard]] std::vector<const domain::ExperienceAtom*> select_candidates(
      const domain::Opportunity& opportunity, const std::vector<domain::ExperienceAtom>& atoms,
      const embedding::IEmbeddingProvider* embedding_provider,
      const vector::IEmbeddingIndex* vector_index, domain::RetrievalStats& stats) const;
};

}  // namespace ccmcp::matching
