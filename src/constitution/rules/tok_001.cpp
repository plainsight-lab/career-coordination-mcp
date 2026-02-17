#include "ccmcp/constitution/rules/tok_001.h"

#include "ccmcp/constitution/rule.h"
#include "ccmcp/constitution/token_ir_artifact_view.h"

namespace ccmcp::constitution {

std::vector<Finding> Tok001::Validate(const ArtifactEnvelope& envelope,
                                      const ValidationContext& /*context*/) const {
  std::vector<Finding> findings;

  // Check if artifact exists
  if (!envelope.artifact) {
    findings.push_back(
        Finding{std::string(rule_id()), FindingSeverity::kBlock, "Missing artifact view", {}});
    return findings;
  }

  // Check artifact type
  if (envelope.artifact_type() != ArtifactType::kResumeTokenIR) {
    findings.push_back(Finding{std::string(rule_id()),
                               FindingSeverity::kBlock,
                               "Invalid artifact type (expected ResumeTokenIR)",
                               {}});
    return findings;
  }

  // Downcast to TokenIRArtifactView
  const auto* token_ir_view = dynamic_cast<const TokenIRArtifactView*>(envelope.artifact.get());
  if (!token_ir_view) {
    findings.push_back(Finding{std::string(rule_id()),
                               FindingSeverity::kBlock,
                               "Failed to cast artifact to TokenIRArtifactView",
                               {}});
    return findings;
  }

  const auto& token_ir = token_ir_view->token_ir();
  const auto& canonical_hash = token_ir_view->canonical_resume_hash();

  // Validate source_hash matches canonical resume hash
  if (token_ir.source_hash != canonical_hash) {
    findings.push_back(Finding{std::string(rule_id()),
                               FindingSeverity::kBlock,
                               "Token IR source_hash does not match canonical resume hash",
                               {}});
  }

  return findings;
}

}  // namespace ccmcp::constitution
