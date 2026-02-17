#include "ccmcp/constitution/rules/tok_003.h"

#include "ccmcp/constitution/rule.h"
#include "ccmcp/constitution/token_ir_artifact_view.h"

#include <algorithm>

namespace ccmcp::constitution {

std::vector<Finding> Tok003::Validate(const ArtifactEnvelope& envelope,
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

  // Count lines in canonical resume if available
  std::size_t max_line = 0;
  if (!canonical_text.empty()) {
    max_line = 1 + std::count(canonical_text.begin(), canonical_text.end(), '\n');
  }

  // Validate each span
  for (std::size_t i = 0; i < token_ir.spans.size(); ++i) {
    const auto& span = token_ir.spans[i];

    // Check start_line >= 1
    if (span.start_line < 1) {
      findings.push_back(Finding{std::string(rule_id()),
                                 FindingSeverity::kFail,
                                 "Span[" + std::to_string(i) + "] has start_line < 1 (" +
                                     std::to_string(span.start_line) + ")",
                                 {}});
    }

    // Check end_line >= 1
    if (span.end_line < 1) {
      findings.push_back(Finding{std::string(rule_id()),
                                 FindingSeverity::kFail,
                                 "Span[" + std::to_string(i) + "] has end_line < 1 (" +
                                     std::to_string(span.end_line) + ")",
                                 {}});
    }

    // Check start_line <= end_line
    if (span.start_line > span.end_line) {
      findings.push_back(Finding{std::string(rule_id()),
                                 FindingSeverity::kFail,
                                 "Span[" + std::to_string(i) + "] has start_line (" +
                                     std::to_string(span.start_line) + ") > end_line (" +
                                     std::to_string(span.end_line) + ")",
                                 {}});
    }

    // Check bounds if we have canonical text
    if (max_line > 0) {
      if (span.end_line > max_line) {
        findings.push_back(Finding{
            std::string(rule_id()),
            FindingSeverity::kFail,
            "Span[" + std::to_string(i) + "] has end_line (" + std::to_string(span.end_line) +
                ") > canonical resume line count (" + std::to_string(max_line) + ")",
            {}});
      }
    }
  }

  return findings;
}

}  // namespace ccmcp::constitution
