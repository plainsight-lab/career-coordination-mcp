#include "ccmcp/constitution/rules/tok_005.h"

#include "ccmcp/constitution/rule.h"
#include "ccmcp/constitution/token_ir_artifact_view.h"

namespace ccmcp::constitution {

std::vector<Finding> Tok005::Validate(const ArtifactEnvelope& envelope,
                                      const ValidationContext& /*context*/) const {
  std::vector<Finding> findings;

  // Check if artifact exists
  if (!envelope.artifact) {
    findings.push_back(
        Finding{std::string(rule_id()), FindingSeverity::kWarn, "Missing artifact view", {}});
    return findings;
  }

  // Check artifact type
  if (envelope.artifact_type() != ArtifactType::kResumeTokenIR) {
    findings.push_back(Finding{std::string(rule_id()),
                               FindingSeverity::kWarn,
                               "Invalid artifact type (expected ResumeTokenIR)",
                               {}});
    return findings;
  }

  // Downcast to TokenIRArtifactView
  const auto* token_ir_view = dynamic_cast<const TokenIRArtifactView*>(envelope.artifact.get());
  if (!token_ir_view) {
    findings.push_back(Finding{std::string(rule_id()),
                               FindingSeverity::kWarn,
                               "Failed to cast artifact to TokenIRArtifactView",
                               {}});
    return findings;
  }

  const auto& token_ir = token_ir_view->token_ir();

  // Count total tokens across all categories
  size_t total_tokens = 0;
  for (const auto& [category, token_list] : token_ir.tokens) {
    total_tokens += token_list.size();

    // Check per-category threshold
    if (token_list.size() > kMaxCategoryTokens) {
      findings.push_back(
          Finding{std::string(rule_id()),
                  FindingSeverity::kWarn,
                  "Category '" + category + "' has " + std::to_string(token_list.size()) +
                      " tokens, exceeds threshold (" + std::to_string(kMaxCategoryTokens) + ")",
                  {}});
    }
  }

  // Check total token threshold
  if (total_tokens > kMaxTotalTokens) {
    findings.push_back(Finding{std::string(rule_id()),
                               FindingSeverity::kWarn,
                               "Total token count (" + std::to_string(total_tokens) +
                                   ") exceeds threshold (" + std::to_string(kMaxTotalTokens) + ")",
                               {}});
  }

  return findings;
}

}  // namespace ccmcp::constitution
