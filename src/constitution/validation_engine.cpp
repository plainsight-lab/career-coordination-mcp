#include "ccmcp/constitution/validation_engine.h"

#include <utility>

#include "ccmcp/constitution/finding.h"

namespace ccmcp::constitution {

ValidationEngine::ValidationEngine(Constitution constitution)
    : constitution_(std::move(constitution)) {}

ValidationReport ValidationEngine::validate(
    const ArtifactEnvelope& envelope,
    const ValidationContext& context) const {
  ValidationReport report{};
  report.report_id = "report-" + envelope.artifact_id;
  report.trace_id = context.trace_id;
  report.artifact_id = envelope.artifact_id;
  report.constitution_id = context.constitution_id;
  report.constitution_version = context.constitution_version;
  report.status = ValidationStatus::kAccepted;

  for (const auto& rule : constitution_.rules) {
    if (!rule.evaluate) {
      continue;
    }
    auto findings = rule.evaluate(envelope, context);
    for (const auto& finding : findings) {
      if (finding.severity == FindingSeverity::kBlock) {
        report.status = ValidationStatus::kBlocked;
      } else if (finding.severity == FindingSeverity::kFail && report.status != ValidationStatus::kBlocked) {
        report.status = ValidationStatus::kRejected;
      } else if (finding.severity == FindingSeverity::kWarn && report.status == ValidationStatus::kAccepted) {
        report.status = ValidationStatus::kNeedsReview;
      }
      report.findings.push_back(finding);
    }
  }

  return report;
}

Constitution make_default_constitution() {
  Rule no_ungrounded_claims{};
  no_ungrounded_claims.rule_id = "GND-001";
  no_ungrounded_claims.version = "0.1.0";
  no_ungrounded_claims.description = "No ungrounded claims";
  no_ungrounded_claims.evaluate = [](const ArtifactEnvelope&, const ValidationContext&) {
    // Stub rule currently always passes while preserving deterministic evaluation shape.
    return std::vector<Finding>{Finding{"GND-001", FindingSeverity::kPass, "Stub pass", {}}};
  };

  Constitution constitution{};
  constitution.constitution_id = "default";
  constitution.version = "0.1.0";
  constitution.rules = {no_ungrounded_claims};
  return constitution;
}

}  // namespace ccmcp::constitution
