#include "ccmcp/constitution/rules/tok_002.h"

#include "ccmcp/constitution/rule.h"
#include "ccmcp/constitution/token_ir_artifact_view.h"

#include <algorithm>

namespace ccmcp::constitution {

std::vector<Finding> Tok002::Validate(const ArtifactEnvelope& envelope,
                                      const ValidationContext& /*context*/) const {
  std::vector<Finding> findings;

  // Check if artifact exists
  if (!envelope.artifact) {
    findings.push_back(
        Finding{std::string(rule_id()), FindingSeverity::kFail, "Missing artifact view", {}});
    return findings;
  }

  // Check artifact type
  if (envelope.artifact_type() != ArtifactType::kResumeTokenIR) {
    findings.push_back(Finding{std::string(rule_id()),
                               FindingSeverity::kFail,
                               "Invalid artifact type (expected ResumeTokenIR)",
                               {}});
    return findings;
  }

  // Downcast to TokenIRArtifactView
  const auto* token_ir_view = dynamic_cast<const TokenIRArtifactView*>(envelope.artifact.get());
  if (!token_ir_view) {
    findings.push_back(Finding{std::string(rule_id()),
                               FindingSeverity::kFail,
                               "Failed to cast artifact to TokenIRArtifactView",
                               {}});
    return findings;
  }

  const auto& token_ir = token_ir_view->token_ir();

  // Validate each token in each category
  for (const auto& [category, token_list] : token_ir.tokens) {
    for (std::size_t i = 0; i < token_list.size(); ++i) {
      const auto& token = token_list[i];

      // Check minimum length
      if (token.size() < kMinTokenLength) {
        findings.push_back(Finding{std::string(rule_id()),
                                   FindingSeverity::kFail,
                                   "Token '" + token + "' in category '" + category +
                                       "' has length < " + std::to_string(kMinTokenLength),
                                   {}});
        continue;
      }

      // Check if token is lowercase ASCII alphanumeric
      bool is_valid = std::all_of(token.begin(), token.end(), [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
      });

      if (!is_valid) {
        findings.push_back(Finding{std::string(rule_id()),
                                   FindingSeverity::kFail,
                                   "Token '" + token + "' in category '" + category +
                                       "' contains non-lowercase-ASCII-alphanumeric characters",
                                   {}});
      }
    }
  }

  return findings;
}

}  // namespace ccmcp::constitution
