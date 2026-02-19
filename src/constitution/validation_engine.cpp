#include "ccmcp/constitution/validation_engine.h"

#include "ccmcp/constitution/finding.h"
#include "ccmcp/constitution/override_request.h"
#include "ccmcp/constitution/rule.h"
#include "ccmcp/constitution/rules/evid_001.h"
#include "ccmcp/constitution/rules/schema_001.h"
#include "ccmcp/constitution/rules/score_001.h"
#include "ccmcp/core/hashing.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace ccmcp::constitution {

namespace {

// Compute validation status from collected findings based on maximum severity.
// Status precedence: BLOCK > FAIL > WARN > PASS
ValidationStatus compute_status_from_findings(const std::vector<Finding>& findings) {
  if (findings.empty()) {
    return ValidationStatus::kAccepted;
  }

  // Find the most severe finding using std::max_element
  auto max_finding =
      std::max_element(findings.begin(), findings.end(),
                       [](const Finding& a, const Finding& b) { return a.severity < b.severity; });

  // Map maximum severity to validation status
  switch (max_finding->severity) {
    case FindingSeverity::kBlock:
      return ValidationStatus::kBlocked;
    case FindingSeverity::kFail:
      return ValidationStatus::kRejected;
    case FindingSeverity::kWarn:
      return ValidationStatus::kNeedsReview;
    case FindingSeverity::kPass:
      return ValidationStatus::kAccepted;
  }

  return ValidationStatus::kAccepted;  // Default case (should not reach here)
}

}  // namespace

ValidationEngine::ValidationEngine(Constitution constitution)
    : constitution_(std::move(constitution)) {}

ValidationReport ValidationEngine::validate(
    const ArtifactEnvelope& envelope, const ValidationContext& context,
    const std::optional<ConstitutionOverrideRequest>& override) const {
  ValidationReport report{};
  report.report_id = "report-" + envelope.artifact_id;
  report.trace_id = context.trace_id;
  report.artifact_id = envelope.artifact_id;
  report.constitution_id = context.constitution_id;
  report.constitution_version = context.constitution_version;

  // Collect all findings from all rules (deterministic iteration order)
  for (const auto& rule : constitution_.rules) {
    if (!rule) {
      continue;  // Skip null rules
    }

    auto findings = rule->Validate(envelope, context);
    report.findings.insert(report.findings.end(), findings.begin(), findings.end());
  }

  // Compute status from collected findings
  report.status = compute_status_from_findings(report.findings);

  // Sort findings deterministically: severity (BLOCK > FAIL > WARN > PASS), then rule_id
  std::sort(report.findings.begin(), report.findings.end(), [](const Finding& a, const Finding& b) {
    // Sort by severity descending (higher severity first)
    if (a.severity != b.severity) {
      return a.severity > b.severity;  // kBlock=3, kFail=2, kWarn=1, kPass=0
    }
    // Then by rule_id lexicographically
    return a.rule_id < b.rule_id;
  });

  // Apply BLOCK override if requested and payload binding matches.
  // Conditions (both required):
  //   1. report.status == kBlocked (at least one BLOCK finding exists)
  //   2. override.rule_id matches a BLOCK finding's rule_id
  //   3. override.payload_hash == stable_hash64_hex(envelope.artifact_id)
  // When applied: status → kOverridden; BLOCK findings remain (audit trail preserved).
  if (override.has_value() && report.status == ValidationStatus::kBlocked) {
    const std::string expected_hash = core::stable_hash64_hex(envelope.artifact_id);
    for (const auto& finding : report.findings) {
      if (finding.severity == FindingSeverity::kBlock && finding.rule_id == override->rule_id &&
          override->payload_hash == expected_hash) {
        report.status = ValidationStatus::kOverridden;
        break;
      }
    }
  }

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
