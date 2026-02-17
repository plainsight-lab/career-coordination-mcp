#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace ccmcp::domain {

/// Tokenizer type identifier
enum class TokenizerType {
  kDeterministicLexical,  // Fallback deterministic tokenizer
  kInferenceAssisted      // LLM-assisted semantic tokenizer
};

/// Tokenizer metadata
struct TokenizerMetadata {
  TokenizerType type;
  std::optional<std::string> model_id;        // e.g., "claude-sonnet-4.5"
  std::optional<std::string> prompt_version;  // e.g., "resume-tokenizer-v1"
};

/// Token span (line reference in canonical resume markdown)
struct TokenSpan {
  std::string token;
  uint32_t start_line;  // 1-indexed
  uint32_t end_line;    // 1-indexed, inclusive
};

/// Resume Token IR - Derived semantic layer
struct ResumeTokenIR {
  std::string schema_version{"0.3"};  // Token IR schema version
  std::string source_hash;            // Binds to canonical resume hash

  TokenizerMetadata tokenizer;

  // Token categories (sorted, deduplicated lowercase ASCII)
  std::map<std::string, std::vector<std::string>> tokens;
  // Common categories: "skills", "domains", "entities", "roles", "artifacts", "outcomes"
  // Deterministic lexical uses: "lexical"

  // Optional spans (line references)
  std::vector<TokenSpan> spans;
};

/// Convert TokenizerType to string
[[nodiscard]] std::string tokenizer_type_to_string(TokenizerType type);

/// Convert string to TokenizerType
[[nodiscard]] std::optional<TokenizerType> string_to_tokenizer_type(const std::string& str);

}  // namespace ccmcp::domain
