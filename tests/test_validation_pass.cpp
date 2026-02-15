#include "ccmcp/constitution/match_report_view.h"
#include "ccmcp/constitution/rule.h"
#include "ccmcp/constitution/validation_engine.h"
#include "ccmcp/core/ids.h"
#include "ccmcp/domain/match_report.h"

#include <catch2/catch_test_macros.hpp>

#include <memory>

using namespace ccmcp::constitution;

TEST_CASE("Validation passes for fully valid MatchReport", "[validation][pass]") {
  SECTION("valid MatchReport with matched requirements passes") {
    // Create fully valid MatchReport
    ccmcp::domain::MatchReport report;
    report.overall_score = 0.75;

    // Valid matched requirement
    ccmcp::domain::RequirementMatch req_match1;
    req_match1.requirement_text = "Python experience";
    req_match1.matched = true;
    req_match1.best_score = 0.8;
    req_match1.contributing_atom_id = ccmcp::core::new_atom_id();
    req_match1.evidence_tokens = {"experience", "python"};

    // Valid unmatched requirement
    ccmcp::domain::RequirementMatch req_match2;
    req_match2.requirement_text = "Rust experience";
    req_match2.matched = false;
    req_match2.best_score = 0.0;
    req_match2.evidence_tokens = {};

    // Partially matched requirement
    ccmcp::domain::RequirementMatch req_match3;
    req_match3.requirement_text = "Cloud architecture";
    req_match3.matched = true;
    req_match3.best_score = 0.5;
    req_match3.contributing_atom_id = ccmcp::core::new_atom_id();
    req_match3.evidence_tokens = {"architecture"};

    report.requirement_matches.push_back(req_match1);
    report.requirement_matches.push_back(req_match2);
    report.requirement_matches.push_back(req_match3);

    // Create envelope with typed artifact view
    auto view = std::make_shared<MatchReportView>(report);
    ArtifactEnvelope envelope;
    envelope.artifact_id = "test-report-pass-1";
    envelope.artifact = view;

    ValidationContext context;
    context.constitution_id = "default";
    context.constitution_version = "0.1.0";
    context.trace_id = "test-trace-pass-1";

    ValidationEngine engine(std::move(make_default_constitution()));
    auto validation_report = engine.validate(envelope, context);

    // Expect ACCEPTED status
    REQUIRE(validation_report.status == ValidationStatus::kAccepted);

    // Should have no BLOCK, FAIL, or WARN findings (only PASS if any)
    for (const auto& finding : validation_report.findings) {
      REQUIRE(finding.severity == FindingSeverity::kPass);
    }
  }

  SECTION("findings are sorted deterministically") {
    // Create report with multiple violations to test sorting
    ccmcp::domain::MatchReport report;
    report.overall_score = 0.0;  // Will trigger SCORE-001 WARN

    // Matched requirement missing evidence (EVID-001 FAIL)
    ccmcp::domain::RequirementMatch req_match1;
    req_match1.requirement_text = "Test1";
    req_match1.matched = true;
    req_match1.best_score = 0.5;
    req_match1.contributing_atom_id = ccmcp::core::new_atom_id();
    req_match1.evidence_tokens = {};  // Missing evidence

    // Invalid schema (SCHEMA-001 BLOCK)
    ccmcp::domain::RequirementMatch req_match2;
    req_match2.requirement_text = "Test2";
    req_match2.matched = true;
    req_match2.best_score = 0.5;
    // Missing contributing_atom_id (SCHEMA violation)
    req_match2.evidence_tokens = {};

    report.requirement_matches.push_back(req_match1);
    report.requirement_matches.push_back(req_match2);

    // Create envelope with typed artifact view
    auto view = std::make_shared<MatchReportView>(report);
    ArtifactEnvelope envelope;
    envelope.artifact_id = "test-report-sort";
    envelope.artifact = view;

    ValidationContext context;
    context.constitution_id = "default";
    context.constitution_version = "0.1.0";
    context.trace_id = "test-trace-sort";

    ValidationEngine engine(std::move(make_default_constitution()));
    auto validation_report = engine.validate(envelope, context);

    REQUIRE(validation_report.status == ValidationStatus::kBlocked);

    // Findings should be sorted: BLOCK > FAIL > WARN, then by rule_id
    bool found_block = false;
    bool found_fail = false;
    bool found_warn = false;

    for (const auto& finding : validation_report.findings) {
      if (finding.severity == FindingSeverity::kBlock) {
        // BLOCK should come first
        REQUIRE(!found_fail);
        REQUIRE(!found_warn);
        found_block = true;
      } else if (finding.severity == FindingSeverity::kFail) {
        // FAIL should come after BLOCK but before WARN
        REQUIRE(!found_warn);
        found_fail = true;
      } else if (finding.severity == FindingSeverity::kWarn) {
        // WARN should come last
        found_warn = true;
      }
    }

    REQUIRE(found_block);
    REQUIRE(found_fail);
    REQUIRE(found_warn);
  }
}
