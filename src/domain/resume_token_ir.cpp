#include "ccmcp/domain/resume_token_ir.h"

namespace ccmcp::domain {

std::string tokenizer_type_to_string(TokenizerType type) {
  switch (type) {
    case TokenizerType::kDeterministicLexical:
      return "deterministic-lexical";
    case TokenizerType::kInferenceAssisted:
      return "inference-assisted";
  }
  return "unknown";
}

std::optional<TokenizerType> string_to_tokenizer_type(const std::string& str) {
  if (str == "deterministic-lexical") {
    return TokenizerType::kDeterministicLexical;
  }
  if (str == "inference-assisted") {
    return TokenizerType::kInferenceAssisted;
  }
  return std::nullopt;
}

}  // namespace ccmcp::domain
