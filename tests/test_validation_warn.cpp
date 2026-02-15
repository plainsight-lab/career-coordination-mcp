#include "ccmcp/constitution/match_report_view.h"
#include "ccmcp/constitution/rule.h"
#include "ccmcp/constitution/validation_engine.h"
#include "ccmcp/domain/match_report.h"

#include <catch2/catch_test_macros.hpp>

#include <memory>

using namespace ccmcp::constitution;

TEST_CASE("Validation warns on degenerate scoring", "[validation][warn]") {
  SECTION("overall_score=0 with requirements triggers WARN") {
    // Create report with zero score but has requirements
    ccmcp::domain::MatchReport report;
    report.overall_score = 0.0;  // Zero score

    ccmcp::domain::RequirementMatch req_match;
    req_match.requirement_text = "Python experience";
    req_match.matched = false;
    req_match.best_score = 0.0;
    req_match.evidence_tokens = {};

    report.requirement_matches.push_back(req_match);  // At least one requirement

    // Create envelope with typed artifact view
    auto view = std::make_shared<MatchReportView>(report);
    ArtifactEnvelope envelope;
    envelope.artifact_id = "test-report-warn-1";
    envelope.artifact = view;

    ValidationContext context;
    context.constitution_id = "default";
    context.constitution_version = "0.1.0";
    context.trace_id = "test-trace-warn-1";

    ValidationEngine engine(std::move(make_default_constitution()));
    auto validation_report = engine.validate(envelope, context);

    // Expect NEEDS_REVIEW status (WARN severity)
    REQUIRE(validation_report.status == ValidationStatus::kNeedsReview);

    // Expect SCORE-001 WARN finding
    bool found_score_warn = false;
    for (const auto& finding : validation_report.findings) {
      if (finding.rule_id == "SCORE-001" && finding.severity == FindingSeverity::kWarn) {
        found_score_warn = true;
      }
    }
    REQUIRE(found_score_warn);
  }

  SECTION("overall_score=0 with NO requirements does not warn") {
    // Create report with zero score and no requirements
    ccmcp::domain::MatchReport report;
    report.overall_score = 0.0;
    report.requirement_matches = {};  // Empty

    // Create envelope with typed artifact view
    auto view = std::make_shared<MatchReportView>(report);
    ArtifactEnvelope envelope;
    envelope.artifact_id = "test-report-warn-2";
    envelope.artifact = view;

    ValidationContext context;
    context.constitution_id = "default";
    context.constitution_version = "0.1.0";
    context.trace_id = "test-trace-warn-2";

    ValidationEngine engine(std::move(make_default_constitution()));
    auto validation_report = engine.validate(envelope, context);

    // Should be ACCEPTED (no warnings for zero requirements)
    REQUIRE(validation_report.status == ValidationStatus::kAccepted);

    // Should NOT have SCORE-001 findings
    for (const auto& finding : validation_report.findings) {
      REQUIRE(finding.rule_id != "SCORE-001");
    }
  }
}
