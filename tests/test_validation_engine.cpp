#include "ccmcp/constitution/validation_engine.h"

#include "ccmcp/constitution/match_report_view.h"
#include "ccmcp/constitution/rule.h"
#include "ccmcp/domain/match_report.h"

#include <catch2/catch_test_macros.hpp>

#include <memory>

TEST_CASE("Default validation engine has active rules", "[validation]") {
  SECTION("constitution contains SCHEMA-001, EVID-001, SCORE-001") {
    auto constitution = ccmcp::constitution::make_default_constitution();

    REQUIRE(constitution.constitution_id == "default");
    REQUIRE(constitution.version == "0.1.0");
    REQUIRE(constitution.rules.size() == 3);

    // Rules in evaluation order (using polymorphic interface)
    REQUIRE(constitution.rules[0]->rule_id() == "SCHEMA-001");
    REQUIRE(constitution.rules[1]->rule_id() == "EVID-001");
    REQUIRE(constitution.rules[2]->rule_id() == "SCORE-001");
  }

  SECTION("missing artifact view triggers SCHEMA-001 BLOCK") {
    auto constitution = ccmcp::constitution::make_default_constitution();
    const ccmcp::constitution::ValidationEngine engine{std::move(constitution)};

    // Create envelope with no artifact view (nullptr)
    ccmcp::constitution::ArtifactEnvelope envelope{};
    envelope.artifact_id = "artifact-1";
    envelope.artifact = nullptr;  // No artifact view provided

    ccmcp::constitution::ValidationContext context{};
    context.constitution_id = constitution.constitution_id;
    context.constitution_version = constitution.version;
    context.trace_id = "trace-1";

    const auto report = engine.validate(envelope, context);

    // Missing artifact should trigger SCHEMA-001 BLOCK
    REQUIRE(report.status == ccmcp::constitution::ValidationStatus::kBlocked);

    // Should have SCHEMA-001 finding
    bool found_schema_block = false;
    for (const auto& finding : report.findings) {
      if (finding.rule_id == "SCHEMA-001" &&
          finding.severity == ccmcp::constitution::FindingSeverity::kBlock) {
        found_schema_block = true;
      }
    }
    REQUIRE(found_schema_block);
  }
}
