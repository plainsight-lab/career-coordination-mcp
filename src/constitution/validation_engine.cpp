#include "ccmcp/constitution/validation_engine.h"

#include "ccmcp/constitution/finding.h"
#include "ccmcp/constitution/rule.h"
#include "ccmcp/constitution/rules/evid_001.h"
#include "ccmcp/constitution/rules/schema_001.h"
#include "ccmcp/constitution/rules/score_001.h"

#include <algorithm>
#include <memory>
#include <utility>

namespace ccmcp::constitution {

ValidationEngine::ValidationEngine(Constitution constitution)
    : constitution_(std::move(constitution)) {}

ValidationReport ValidationEngine::validate(const ArtifactEnvelope& envelope,
                                            const ValidationContext& context) const {
  ValidationReport report{};
  report.report_id = "report-" + envelope.artifact_id;
  report.trace_id = context.trace_id;
  report.artifact_id = envelope.artifact_id;
  report.constitution_id = context.constitution_id;
  report.constitution_version = context.constitution_version;
  report.status = ValidationStatus::kAccepted;

  // Evaluate each rule in order (deterministic iteration)
  for (const auto& rule : constitution_.rules) {
    if (!rule) {
      continue;  // Skip null rules
    }

    auto findings = rule->Validate(envelope, context);

    // Aggregate findings and update status
    for (const auto& finding : findings) {
      if (finding.severity == FindingSeverity::kBlock) {
        report.status = ValidationStatus::kBlocked;
      } else if (finding.severity == FindingSeverity::kFail &&
                 report.status != ValidationStatus::kBlocked) {
        report.status = ValidationStatus::kRejected;
      } else if (finding.severity == FindingSeverity::kWarn &&
                 report.status == ValidationStatus::kAccepted) {
        report.status = ValidationStatus::kNeedsReview;
      }
      report.findings.push_back(finding);
    }
  }

  // Sort findings deterministically: severity (BLOCK > FAIL > WARN > PASS), then rule_id
  std::sort(report.findings.begin(), report.findings.end(), [](const Finding& a, const Finding& b) {
    // Sort by severity descending (higher severity first)
    if (a.severity != b.severity) {
      return a.severity > b.severity;  // kBlock=3, kFail=2, kWarn=1, kPass=0
    }
    // Then by rule_id lexicographically
    return a.rule_id < b.rule_id;
  });

  return report;
}

Constitution make_default_constitution() {
  Constitution constitution{};
  constitution.constitution_id = "default";
  constitution.version = "0.1.0";

  // Rules in fixed evaluation order (deterministic)
  constitution.rules.push_back(std::make_unique<Schema001>());
  constitution.rules.push_back(std::make_unique<Evid001>());
  constitution.rules.push_back(std::make_unique<Score001>());

  return constitution;
}

}  // namespace ccmcp::constitution
