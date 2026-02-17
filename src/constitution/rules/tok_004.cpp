#include "ccmcp/constitution/rules/tok_004.h"

#include "ccmcp/constitution/rule.h"
#include "ccmcp/constitution/token_ir_artifact_view.h"
#include "ccmcp/core/normalization.h"

#include <algorithm>
#include <unordered_set>

namespace ccmcp::constitution {

std::vector<Finding> Tok004::Validate(const ArtifactEnvelope& envelope,
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
  const auto& canonical_text = token_ir_view->canonical_resume_text();

  // Build set of derivable tokens from canonical resume
  std::unordered_set<std::string> derivable_tokens;
  if (!canonical_text.empty()) {
    auto derived = core::tokenize_ascii(canonical_text, 2);
    derivable_tokens.insert(derived.begin(), derived.end());
  }

  // Validate each token in each category
  for (const auto& [category, token_list] : token_ir.tokens) {
    for (const auto& token : token_list) {
      // Check if token is derivable from canonical resume
      if (derivable_tokens.find(token) == derivable_tokens.end()) {
        findings.push_back(Finding{std::string(rule_id()),
                                   FindingSeverity::kFail,
                                   "Token '" + token + "' in category '" + category +
                                       "' is not derivable from canonical resume (hallucinated)",
                                   {}});
      }
    }
  }

  return findings;
}

}  // namespace ccmcp::constitution
