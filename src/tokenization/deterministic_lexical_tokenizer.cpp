#include "ccmcp/tokenization/deterministic_lexical_tokenizer.h"

#include "ccmcp/core/normalization.h"

#include <algorithm>
#include <set>

namespace ccmcp::tokenization {

// Common English stop words - deterministic list for reproducible filtering
// Includes: articles, prepositions, conjunctions, pronouns, common verbs
const std::unordered_set<std::string> DeterministicLexicalTokenizer::kStopWords = {
    // Articles
    "a", "an", "the",

    // Prepositions
    "about", "above", "across", "after", "against", "along", "among", "around", "at", "before",
    "behind", "below", "beneath", "beside", "between", "beyond", "by", "down", "during", "except",
    "for", "from", "in", "inside", "into", "near", "of", "off", "on", "onto", "out", "outside",
    "over", "past", "since", "through", "throughout", "to", "toward", "under", "underneath",
    "until", "up", "upon", "with", "within", "without",

    // Conjunctions
    "and", "but", "or", "nor", "so", "yet", "as", "if", "than", "that", "though", "unless", "until",
    "when", "where", "whether", "while",

    // Pronouns
    "i", "me", "my", "mine", "myself", "you", "your", "yours", "yourself", "he", "him", "his",
    "himself", "she", "her", "hers", "herself", "it", "its", "itself", "we", "us", "our", "ours",
    "ourselves", "they", "them", "their", "theirs", "themselves", "this", "that", "these", "those",
    "who", "whom", "whose", "which", "what",

    // Common verbs (to be, to have, to do, modals)
    "am", "is", "are", "was", "were", "be", "been", "being", "have", "has", "had", "having", "do",
    "does", "did", "doing", "will", "would", "shall", "should", "may", "might", "must", "can",
    "could",

    // Other common words
    "all", "any", "both", "each", "few", "more", "most", "other", "some", "such", "no", "not",
    "only", "own", "same", "then", "there", "very", "get", "got", "make", "made", "just", "like",
    "well", "also", "back", "even", "still", "way", "take", "come", "give", "use", "find", "tell",
    "ask", "work", "seem", "feel", "try", "leave", "call"};

DeterministicLexicalTokenizer::DeterministicLexicalTokenizer(bool filter_stop_words)
    : filter_stop_words_(filter_stop_words) {}

std::vector<std::string> tokenize_deterministic(const std::string& text) {
  // Use existing tokenize_ascii function (lowercase, split, min 2 chars)
  auto tokens = core::tokenize_ascii(text);

  // Deduplicate using set
  std::set<std::string> unique_tokens(tokens.begin(), tokens.end());

  // Convert back to sorted vector
  return std::vector<std::string>(unique_tokens.begin(), unique_tokens.end());
}

domain::ResumeTokenIR DeterministicLexicalTokenizer::tokenize(const std::string& resume_md,
                                                              const std::string& source_hash) {
  domain::ResumeTokenIR ir;

  ir.schema_version = "0.3";
  ir.source_hash = source_hash;

  // Tokenizer metadata
  ir.tokenizer.type = domain::TokenizerType::kDeterministicLexical;
  // No model_id or prompt_version for deterministic tokenizer

  // Generate tokens
  auto tokens = tokenize_deterministic(resume_md);

  // Filter stop words if enabled
  if (filter_stop_words_) {
    tokens.erase(std::remove_if(tokens.begin(), tokens.end(),
                                [](const std::string& token) {
                                  return kStopWords.find(token) != kStopWords.end();
                                }),
                 tokens.end());
  }

  // Store in "lexical" category
  ir.tokens["lexical"] = std::move(tokens);

  // Spans are optional for deterministic tokenizer (not implemented)
  // Rationale: Line-level tracking would require additional parsing;
  // deterministic tokenizer is primarily for fallback/baseline

  return ir;
}

}  // namespace ccmcp::tokenization
