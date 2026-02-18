#include "ccmcp/domain/decision_record.h"

#include <catch2/catch_test_macros.hpp>

using namespace ccmcp::domain;

// Helper: minimal DecisionRecord for use across multiple test cases.
static DecisionRecord make_record(const std::string& decision_id = "decision-001",
                                  const std::string& trace_id = "trace-001") {
  DecisionRecord r;
  r.decision_id = decision_id;
  r.trace_id = trace_id;
  r.artifact_id = "match-report-opp-001";
  r.created_at = "2026-01-01T00:00:00Z";
  r.opportunity_id = "opp-001";
  r.version = "0.3";

  RequirementDecision rd;
  rd.requirement_text = "C++20";
  rd.atom_id = "atom-001";
  rd.evidence_tokens = {"cpp", "cpp20"};
  r.requirement_decisions.push_back(rd);

  r.retrieval_stats.lexical_candidates = 10;
  r.retrieval_stats.embedding_candidates = 5;
  r.retrieval_stats.merged_candidates = 12;

  r.validation_summary.status = "accepted";
  r.validation_summary.finding_count = 1;
  r.validation_summary.fail_count = 0;
  r.validation_summary.warn_count = 1;
  r.validation_summary.top_rule_ids = {"R-WARN-001"};

  return r;
}

TEST_CASE("decision_record_to_json and from_json roundtrip", "[decision-record]") {
  auto original = make_record();
  auto json = decision_record_to_json(original);
  auto restored = decision_record_from_json(json);

  CHECK(restored.decision_id == original.decision_id);
  CHECK(restored.trace_id == original.trace_id);
  CHECK(restored.artifact_id == original.artifact_id);
  REQUIRE(restored.created_at.has_value());
  CHECK(restored.created_at.value() == original.created_at.value());
  CHECK(restored.opportunity_id == original.opportunity_id);
  CHECK(restored.version == original.version);

  REQUIRE(restored.requirement_decisions.size() == 1);
  CHECK(restored.requirement_decisions[0].requirement_text == "C++20");
  REQUIRE(restored.requirement_decisions[0].atom_id.has_value());
  CHECK(restored.requirement_decisions[0].atom_id.value() == "atom-001");
  CHECK(restored.requirement_decisions[0].evidence_tokens ==
        std::vector<std::string>{"cpp", "cpp20"});

  CHECK(restored.retrieval_stats.lexical_candidates == 10);
  CHECK(restored.retrieval_stats.embedding_candidates == 5);
  CHECK(restored.retrieval_stats.merged_candidates == 12);

  CHECK(restored.validation_summary.status == "accepted");
  CHECK(restored.validation_summary.finding_count == 1);
  CHECK(restored.validation_summary.fail_count == 0);
  CHECK(restored.validation_summary.warn_count == 1);
  CHECK(restored.validation_summary.top_rule_ids == std::vector<std::string>{"R-WARN-001"});
}

TEST_CASE("decision_record_to_json is deterministic", "[decision-record]") {
  auto r = make_record();
  const std::string json1 = decision_record_to_json(r).dump();
  const std::string json2 = decision_record_to_json(r).dump();
  CHECK(json1 == json2);
}

TEST_CASE("decision_record null created_at roundtrip", "[decision-record]") {
  auto r = make_record();
  r.created_at = std::nullopt;

  auto json = decision_record_to_json(r);
  CHECK(json["created_at"].is_null());

  auto restored = decision_record_from_json(json);
  CHECK(!restored.created_at.has_value());
}

TEST_CASE("decision_record null atom_id roundtrip", "[decision-record]") {
  RequirementDecision rd;
  rd.requirement_text = "Go experience";
  rd.atom_id = std::nullopt;
  rd.evidence_tokens = {};

  DecisionRecord r = make_record();
  r.requirement_decisions = {rd};

  auto json = decision_record_to_json(r);
  REQUIRE(json["requirement_decisions"].size() == 1);
  CHECK(json["requirement_decisions"][0]["atom_id"].is_null());

  auto restored = decision_record_from_json(json);
  REQUIRE(restored.requirement_decisions.size() == 1);
  CHECK(!restored.requirement_decisions[0].atom_id.has_value());
}

TEST_CASE("decision_record JSON keys are alphabetically ordered", "[decision-record]") {
  auto r = make_record();
  auto json = decision_record_to_json(r);

  // nlohmann::json default object type is std::map → keys are sorted.
  // Verify that the dump starts with "artifact_id" (first alphabetically among top keys).
  const std::string dumped = json.dump();
  // First key in the object should be "artifact_id"
  const std::string expected_prefix = R"({"artifact_id")";
  CHECK(dumped.rfind(expected_prefix, 0) == 0);
}
