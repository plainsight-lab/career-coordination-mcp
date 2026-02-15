#include <catch2/catch_test_macros.hpp>

#include "ccmcp/constitution/match_report_view.h"
#include "ccmcp/constitution/rule.h"
#include "ccmcp/constitution/validation_engine.h"
#include "ccmcp/core/ids.h"
#include "ccmcp/domain/match_report.h"

#include <memory>

using namespace ccmcp::constitution;

TEST_CASE("Validation fails when evidence is missing", "[validation][evidence]") {
  SECTION("matched requirement with empty evidence_tokens triggers FAIL") {
    // Create valid schema but missing evidence
    ccmcp::domain::MatchReport report;
    report.overall_score = 0.5;

    ccmcp::domain::RequirementMatch req_match;
    req_match.requirement_text = "Python experience";
    req_match.matched = true;
    req_match.best_score = 0.5;
    req_match.contributing_atom_id = ccmcp::core::new_atom_id();
    req_match.evidence_tokens = {};  // Empty! (EVID-001 violation)

    report.requirement_matches.push_back(req_match);

    // Create envelope with typed artifact view
    auto view = std::make_shared<MatchReportView>(report);
    ArtifactEnvelope envelope;
    envelope.artifact_id = "test-report-evid-1";
    envelope.artifact = view;

    ValidationContext context;
    context.constitution_id = "default";
    context.constitution_version = "0.1.0";
    context.trace_id = "test-trace-evid-1";

    ValidationEngine engine(std::move(make_default_constitution()));
    auto validation_report = engine.validate(envelope, context);

    // Expect REJECTED status (FAIL severity)
    REQUIRE(validation_report.status == ValidationStatus::kRejected);

    // Expect EVID-001 FAIL finding
    bool found_evid_fail = false;
    for (const auto& finding : validation_report.findings) {
      if (finding.rule_id == "EVID-001" && finding.severity == FindingSeverity::kFail) {
        found_evid_fail = true;
      }
    }
    REQUIRE(found_evid_fail);
  }

  SECTION("unmatched requirement does not trigger evidence check") {
    // Create report with unmatched requirement
    ccmcp::domain::MatchReport report;
    report.overall_score = 0.0;

    ccmcp::domain::RequirementMatch req_match;
    req_match.requirement_text = "Rust experience";
    req_match.matched = false;  // Not matched
    req_match.best_score = 0.0;
    req_match.evidence_tokens = {};  // Empty is OK for unmatched

    report.requirement_matches.push_back(req_match);

    // Create envelope with typed artifact view
    auto view = std::make_shared<MatchReportView>(report);
    ArtifactEnvelope envelope;
    envelope.artifact_id = "test-report-evid-2";
    envelope.artifact = view;

    ValidationContext context;
    context.constitution_id = "default";
    context.constitution_version = "0.1.0";
    context.trace_id = "test-trace-evid-2";

    ValidationEngine engine(std::move(make_default_constitution()));
    auto validation_report = engine.validate(envelope, context);

    // Should get WARN for zero score, not FAIL for evidence
    REQUIRE(validation_report.status == ValidationStatus::kNeedsReview);

    // Should NOT have EVID-001 findings
    for (const auto& finding : validation_report.findings) {
      REQUIRE(finding.rule_id != "EVID-001");
    }
  }
}
