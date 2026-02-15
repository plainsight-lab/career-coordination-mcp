#include "ccmcp/constitution/match_report_view.h"
#include "ccmcp/constitution/rule.h"
#include "ccmcp/constitution/validation_engine.h"
#include "ccmcp/core/ids.h"
#include "ccmcp/domain/match_report.h"

#include <catch2/catch_test_macros.hpp>

#include <memory>

using namespace ccmcp::constitution;

TEST_CASE("Validation blocks on schema violations", "[validation][schema]") {
  SECTION("matched=true but missing contributing_atom_id triggers BLOCK") {
    // Create invalid MatchReport (matched=true but no contributing_atom_id)
    ccmcp::domain::MatchReport report;
    report.overall_score = 0.5;

    ccmcp::domain::RequirementMatch req_match;
    req_match.requirement_text = "Python experience";
    req_match.matched = true;  // matched=true
    req_match.best_score = 0.5;
    // contributing_atom_id is std::nullopt - SCHEMA violation!
    req_match.evidence_tokens = {};

    report.requirement_matches.push_back(req_match);

    // Create envelope with typed artifact view
    auto view = std::make_shared<MatchReportView>(report);
    ArtifactEnvelope envelope;
    envelope.artifact_id = "test-report-1";
    envelope.artifact = view;

    ValidationContext context;
    context.constitution_id = "default";
    context.constitution_version = "0.1.0";
    context.trace_id = "test-trace-1";

    // Validate
    ValidationEngine engine(std::move(make_default_constitution()));
    auto validation_report = engine.validate(envelope, context);

    // Expect BLOCKED status
    REQUIRE(validation_report.status == ValidationStatus::kBlocked);

    // Expect at least one BLOCK finding from SCHEMA-001
    bool found_schema_block = false;
    for (const auto& finding : validation_report.findings) {
      if (finding.rule_id == "SCHEMA-001" && finding.severity == FindingSeverity::kBlock) {
        found_schema_block = true;
      }
    }
    REQUIRE(found_schema_block);
  }

  SECTION("negative best_score triggers BLOCK") {
    // Create invalid MatchReport with negative score
    ccmcp::domain::MatchReport report;
    report.overall_score = 0.0;

    ccmcp::domain::RequirementMatch req_match;
    req_match.requirement_text = "Test requirement";
    req_match.matched = false;
    req_match.best_score = -0.1;  // Invalid negative score - SCHEMA violation!
    req_match.evidence_tokens = {};

    report.requirement_matches.push_back(req_match);

    // Create envelope with typed artifact view
    auto view = std::make_shared<MatchReportView>(report);
    ArtifactEnvelope envelope;
    envelope.artifact_id = "test-report-2";
    envelope.artifact = view;

    ValidationContext context;
    context.constitution_id = "default";
    context.constitution_version = "0.1.0";
    context.trace_id = "test-trace-2";

    ValidationEngine engine(std::move(make_default_constitution()));
    auto validation_report = engine.validate(envelope, context);

    REQUIRE(validation_report.status == ValidationStatus::kBlocked);

    bool found_schema_block = false;
    for (const auto& finding : validation_report.findings) {
      if (finding.rule_id == "SCHEMA-001" && finding.severity == FindingSeverity::kBlock) {
        found_schema_block = true;
      }
    }
    REQUIRE(found_schema_block);
  }
}
