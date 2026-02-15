#include "ccmcp/constitution/rules/score_001.h"

#include "ccmcp/constitution/match_report_view.h"
#include "ccmcp/constitution/rule.h"

namespace ccmcp::constitution {

std::vector<Finding> Score001::Validate(const ArtifactEnvelope& envelope,
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

  // Warn if overall_score == 0.0 AND requirements exist
  if (report.overall_score == 0.0 && !report.requirement_matches.empty()) {
    findings.push_back(Finding{std::string(rule_id()), FindingSeverity::kWarn,
                                "All requirement scores are zero.", {}});
  }

  return findings;
}

}  // namespace ccmcp::constitution
