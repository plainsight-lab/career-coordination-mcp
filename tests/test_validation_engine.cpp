#include <catch2/catch_test_macros.hpp>

#include "ccmcp/constitution/validation_engine.h"

TEST_CASE("Default validation engine returns deterministic stub finding", "[validation]") {
  const auto constitution = ccmcp::constitution::make_default_constitution();
  const ccmcp::constitution::ValidationEngine engine{constitution};

  ccmcp::constitution::ArtifactEnvelope envelope{};
  envelope.artifact_id = "artifact-1";
  envelope.artifact_type = "resume";
  envelope.content = "content";

  ccmcp::constitution::ValidationContext context{};
  context.constitution_id = constitution.constitution_id;
  context.constitution_version = constitution.version;
  context.trace_id = "trace-1";

  const auto report = engine.validate(envelope, context);

  REQUIRE(report.status == ccmcp::constitution::ValidationStatus::kAccepted);
  REQUIRE(report.findings.size() == 1);
  REQUIRE(report.findings.front().rule_id == "GND-001");
}
