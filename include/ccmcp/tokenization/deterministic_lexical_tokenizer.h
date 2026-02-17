#pragma once

#include "ccmcp/tokenization/tokenization_provider.h"

#include <string>
#include <unordered_set>
#include <vector>

namespace ccmcp::tokenization {

/// Deterministic lexical tokenizer (fallback, no ML required)
/// Rules:
/// - ASCII-only lowercase normalization
/// - Replace non-alphanumeric ASCII with delimiters
/// - Split on delimiters
/// - Drop tokens < 2 characters
/// - Filter common English stop words (configurable)
/// - Deduplicate and sort
class DeterministicLexicalTokenizer final : public ITokenizationProvider {
 public:
  /// Constructor
  /// @param filter_stop_words If true, removes common English stop words (default: true)
  explicit DeterministicLexicalTokenizer(bool filter_stop_words = true);

  [[nodiscard]] domain::ResumeTokenIR tokenize(const std::string& resume_md,
                                               const std::string& source_hash) override;

 private:
  bool filter_stop_words_;

  /// Common English stop words (articles, prepositions, conjunctions, etc.)
  /// Deterministic list for reproducible filtering
  static const std::unordered_set<std::string> kStopWords;
};

/// Helper: tokenize text deterministically
[[nodiscard]] std::vector<std::string> tokenize_deterministic(const std::string& text);

}  // namespace ccmcp::tokenization
