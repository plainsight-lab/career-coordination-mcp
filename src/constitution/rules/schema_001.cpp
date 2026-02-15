#include "ccmcp/constitution/rules/schema_001.h"

#include "ccmcp/constitution/match_report_view.h"
#include "ccmcp/constitution/rule.h"

namespace ccmcp::constitution {

std::vector<Finding> Schema001::Validate(const ArtifactEnvelope& envelope,
                                          const ValidationContext& /*context*/) const {
  std::vector<Finding> findings;

  // Check if artifact exists
  if (!envelope.artifact) {
    findings.push_back(
        Finding{std::string(rule_id()), FindingSeverity::kBlock, "Missing artifact view", {}});
    return findings;
  }

  // Check artifact type
  if (envelope.artifact_type() != ArtifactType::kMatchReport) {
    findings.push_back(Finding{std::string(rule_id()), FindingSeverity::kBlock,
                                "Invalid artifact type (expected MatchReport)", {}});
    return findings;
  }

  // Downcast to MatchReportView
  const auto* report_view = dynamic_cast<const MatchReportView*>(envelope.artifact.get());
  if (!report_view) {
    findings.push_back(
        Finding{std::string(rule_id()), FindingSeverity::kBlock,
                "Failed to cast artifact to MatchReportView", {}});
    return findings;
  }

  const auto& report = report_view->report();

  // Check overall_score >= 0
  if (report.overall_score < 0.0) {
    findings.push_back(Finding{std::string(rule_id()), FindingSeverity::kBlock,
                                "overall_score is negative", {}});
  }

  // Validate each RequirementMatch
  for (std::size_t i = 0; i < report.requirement_matches.size(); ++i) {
    const auto& rm = report.requirement_matches[i];

    // Check requirement_text not empty
    if (rm.requirement_text.empty()) {
      findings.push_back(Finding{std::string(rule_id()), FindingSeverity::kBlock,
                                  "RequirementMatch[" + std::to_string(i) +
                                      "] has empty requirement_text",
                                  {}});
    }

    // Check best_score >= 0
    if (rm.best_score < 0.0) {
      findings.push_back(Finding{std::string(rule_id()), FindingSeverity::kBlock,
                                  "RequirementMatch[" + std::to_string(i) +
                                      "] has negative best_score",
                                  {}});
    }

    // Check matched flag consistency
    bool has_contributing_atom = rm.contributing_atom_id.has_value() &&
                                  !rm.contributing_atom_id->value.empty();

    if (rm.matched && !has_contributing_atom) {
      findings.push_back(Finding{std::string(rule_id()), FindingSeverity::kBlock,
                                  "RequirementMatch[" + std::to_string(i) +
                                      "] is matched=true but missing contributing_atom_id",
                                  {}});
    } else if (!rm.matched && has_contributing_atom) {
      findings.push_back(Finding{std::string(rule_id()), FindingSeverity::kBlock,
                                  "RequirementMatch[" + std::to_string(i) +
                                      "] is matched=false but has contributing_atom_id",
                                  {}});
    }
  }

  return findings;
}

}  // namespace ccmcp::constitution
