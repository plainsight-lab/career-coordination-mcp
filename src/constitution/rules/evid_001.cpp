#include "ccmcp/constitution/rules/evid_001.h"

#include "ccmcp/constitution/match_report_view.h"
#include "ccmcp/constitution/rule.h"

namespace ccmcp::constitution {

std::vector<Finding> Evid001::Validate(const ArtifactEnvelope& envelope,
                                       const ValidationContext& /*context*/) const {
  std::vector<Finding> findings;

  // Skip if no artifact or wrong type (SCHEMA-001 will catch this)
  if (!envelope.artifact || envelope.artifact_type() != ArtifactType::kMatchReport) {
    return findings;
  }

  const auto* report_view = dynamic_cast<const MatchReportView*>(envelope.artifact.get());
  if (!report_view) {
    return findings;
  }

  const auto& report = report_view->report();

  // Check each matched requirement for evidence
  for (const auto& rm : report.requirement_matches) {
    if (!rm.matched) {
      continue;  // Only check matched requirements
    }

    // Check contributing_atom_id exists
    if (!rm.contributing_atom_id.has_value() || rm.contributing_atom_id->value.empty()) {
      findings.push_back(
          Finding{std::string(rule_id()),
                  FindingSeverity::kFail,
                  "Matched requirement '" + rm.requirement_text + "' missing contributing_atom_id",
                  {}});
    }

    // Check evidence_tokens not empty
    if (rm.evidence_tokens.empty()) {
      findings.push_back(
          Finding{std::string(rule_id()),
                  FindingSeverity::kFail,
                  "Matched requirement '" + rm.requirement_text + "' has no evidence_tokens",
                  {}});
    }
  }

  return findings;
}

}  // namespace ccmcp::constitution
