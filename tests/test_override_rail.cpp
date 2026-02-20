#include "ccmcp/constitution/constitution.h"
#include "ccmcp/constitution/override_request.h"
#include "ccmcp/constitution/rule.h"
#include "ccmcp/constitution/validation_engine.h"
#include "ccmcp/constitution/validation_report.h"
#include "ccmcp/core/sha256.h"

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <memory>
#include <optional>
#include <string>

// Helper: envelope whose null artifact view triggers SCHEMA-001 BLOCK.
// artifact_id identifies the artifact for payload binding.
static ccmcp::constitution::ArtifactEnvelope make_blocked_envelope(const std::string& artifact_id) {
  ccmcp::constitution::ArtifactEnvelope env;
  env.artifact_id = artifact_id;
  env.artifact = nullptr;  // null view → SCHEMA-001 BLOCK
  return env;
}

static ccmcp::constitution::ValidationContext make_context() {
  ccmcp::constitution::ValidationContext ctx;
  ctx.constitution_id = "default";
  ctx.constitution_version = "0.1.0";
  ctx.trace_id = "trace-override-test";
  return ctx;
}

// ─── Test A / B / C: CVE override logic ────────────────────────────────────

TEST_CASE("BLOCK Override Rail — CVE integration", "[override][validation]") {
  auto constitution = ccmcp::constitution::make_default_constitution();
  const ccmcp::constitution::ValidationEngine engine{std::move(constitution)};

  const std::string artifact_id = "match-report-opp-override-test";
  const auto envelope = make_blocked_envelope(artifact_id);
  const auto context = make_context();
  const std::string correct_hash = ccmcp::core::sha256_hex(artifact_id);

  SECTION("A: BLOCK without override yields kBlocked") {
    const auto report = engine.validate(envelope, context);

    REQUIRE(report.status == ccmcp::constitution::ValidationStatus::kBlocked);
    // Verify SCHEMA-001 BLOCK finding is present
    bool found_block = false;
    for (const auto& f : report.findings) {
      if (f.rule_id == "SCHEMA-001" && f.severity == ccmcp::constitution::FindingSeverity::kBlock) {
        found_block = true;
      }
    }
    REQUIRE(found_block);
  }

  SECTION("B: BLOCK with valid override (matching rule_id + payload_hash) yields kOverridden") {
    ccmcp::constitution::ConstitutionOverrideRequest override_req;
    override_req.rule_id = "SCHEMA-001";
    override_req.operator_id = "operator-alice";
    override_req.reason = "Manually verified structural integrity in offline review";
    override_req.payload_hash = correct_hash;

    const auto report = engine.validate(envelope, context, override_req);

    REQUIRE(report.status == ccmcp::constitution::ValidationStatus::kOverridden);

    // BLOCK finding is preserved in findings (immutable audit trail)
    bool found_block = false;
    for (const auto& f : report.findings) {
      if (f.rule_id == "SCHEMA-001" && f.severity == ccmcp::constitution::FindingSeverity::kBlock) {
        found_block = true;
      }
    }
    REQUIRE(found_block);
  }

  SECTION("C1: override mismatch — wrong rule_id yields kBlocked") {
    ccmcp::constitution::ConstitutionOverrideRequest override_req;
    override_req.rule_id = "EVID-001";  // EVID-001 has no BLOCK finding here
    override_req.operator_id = "operator-alice";
    override_req.reason = "Overriding wrong rule";
    override_req.payload_hash = correct_hash;

    const auto report = engine.validate(envelope, context, override_req);

    // SCHEMA-001 BLOCK remains; EVID-001 override doesn't match
    REQUIRE(report.status == ccmcp::constitution::ValidationStatus::kBlocked);
  }

  SECTION("C2: override mismatch — wrong payload_hash yields kBlocked") {
    ccmcp::constitution::ConstitutionOverrideRequest override_req;
    override_req.rule_id = "SCHEMA-001";
    override_req.operator_id = "operator-alice";
    override_req.reason = "Correct rule, wrong artifact hash";
    override_req.payload_hash = "deadbeef00000000deadbeef00000000";  // Wrong hash

    const auto report = engine.validate(envelope, context, override_req);

    REQUIRE(report.status == ccmcp::constitution::ValidationStatus::kBlocked);
  }
}

// ─── Test D: ConstitutionOverrideRequest serialization ──────────────────────

TEST_CASE("ConstitutionOverrideRequest — deterministic serialization",
          "[override][serialization]") {
  SECTION("D1: override_request_to_json produces deterministic alphabetically sorted keys") {
    ccmcp::constitution::ConstitutionOverrideRequest req;
    req.rule_id = "SCHEMA-001";
    req.operator_id = "operator-alice";
    req.reason = "Manually verified structural integrity";
    req.payload_hash = "abc123def456";

    const auto j1 = ccmcp::constitution::override_request_to_json(req);
    const auto j2 = ccmcp::constitution::override_request_to_json(req);

    // Same input → same JSON string (deterministic)
    REQUIRE(j1.dump() == j2.dump());

    // Keys are alphabetically sorted:
    // binding_hash_alg < operator_id < payload_hash < reason < rule_id
    const std::string serialized = j1.dump();
    const auto pos_binding = serialized.find("binding_hash_alg");
    const auto pos_operator = serialized.find("operator_id");
    const auto pos_payload = serialized.find("payload_hash");
    const auto pos_reason = serialized.find("reason");
    const auto pos_rule = serialized.find("rule_id");

    REQUIRE(pos_binding != std::string::npos);
    REQUIRE(pos_operator != std::string::npos);
    REQUIRE(pos_payload != std::string::npos);
    REQUIRE(pos_reason != std::string::npos);
    REQUIRE(pos_rule != std::string::npos);

    REQUIRE(pos_binding < pos_operator);
    REQUIRE(pos_operator < pos_payload);
    REQUIRE(pos_payload < pos_reason);
    REQUIRE(pos_reason < pos_rule);
  }

  SECTION("D2: override_request_from_json round-trips correctly") {
    ccmcp::constitution::ConstitutionOverrideRequest original;
    original.rule_id = "TOK-001";
    original.operator_id = "operator-bob";
    original.reason = "Source hash mismatch was caused by a known tooling bug";
    original.payload_hash = "feedcafe00000001";
    original.binding_hash_alg = "sha256";

    const auto j = ccmcp::constitution::override_request_to_json(original);
    const auto restored = ccmcp::constitution::override_request_from_json(j);

    REQUIRE(restored.rule_id == original.rule_id);
    REQUIRE(restored.operator_id == original.operator_id);
    REQUIRE(restored.reason == original.reason);
    REQUIRE(restored.payload_hash == original.payload_hash);
    REQUIRE(restored.binding_hash_alg == original.binding_hash_alg);
  }

  SECTION("D3: payload_hash is deterministic — sha256_hex same input → same output") {
    const std::string artifact_id = "match-report-opp-serial-test";
    const std::string hash_a = ccmcp::core::sha256_hex(artifact_id);
    const std::string hash_b = ccmcp::core::sha256_hex(artifact_id);

    REQUIRE(hash_a == hash_b);
    REQUIRE(!hash_a.empty());
    // Different artifact IDs produce different hashes
    const std::string hash_c = ccmcp::core::sha256_hex("match-report-different");
    REQUIRE(hash_a != hash_c);
  }
}
