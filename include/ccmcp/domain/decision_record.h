#pragma once

#include <nlohmann/json.hpp>

#include <optional>
#include <string>
#include <vector>

namespace ccmcp::domain {

// Per-requirement match evidence captured in a DecisionRecord.
// Mirrors RequirementMatch but records only "why" fields — no scores.
struct RequirementDecision {
  std::string requirement_text;              // NOLINT(readability-identifier-naming)
  std::optional<std::string> atom_id;        // nullopt when requirement was not matched
  std::vector<std::string> evidence_tokens;  // NOLINT(readability-identifier-naming)
};

// Snapshot of retrieval provenance from the MatchReport.
struct RetrievalStatsSummary {
  size_t lexical_candidates{0};    // NOLINT(readability-identifier-naming)
  size_t embedding_candidates{0};  // NOLINT(readability-identifier-naming)
  size_t merged_candidates{0};     // NOLINT(readability-identifier-naming)
};

// Summary of constitutional validation outcome.
struct ValidationSummary {
  std::string status;                     // NOLINT(readability-identifier-naming)
  size_t finding_count{0};                // NOLINT(readability-identifier-naming)
  size_t fail_count{0};                   // NOLINT(readability-identifier-naming)
  size_t warn_count{0};                   // NOLINT(readability-identifier-naming)
  std::vector<std::string> top_rule_ids;  // sorted; from fail+block+warn findings
                                          // NOLINT(readability-identifier-naming)
};

// DecisionRecord captures the "why" of a match decision.
// It is a separate, append-only artifact — it does not modify MatchReport.
// Serialized as JSON for opaque storage in SQLite; fields are the query surface.
//
// Version: "0.3"
// Created: immediately after run_match_pipeline() produces MatchReport + ValidationReport.
struct DecisionRecord {
  std::string decision_id;                                 // NOLINT(readability-identifier-naming)
  std::string trace_id;                                    // links to audit trail
  std::string artifact_id;                                 // "match-report-{opportunity_id}"
  std::optional<std::string> created_at;                   // nullable for deterministic tests
  std::string opportunity_id;                              // NOLINT(readability-identifier-naming)
  std::vector<RequirementDecision> requirement_decisions;  // in requirement order
  RetrievalStatsSummary retrieval_stats;                   // NOLINT(readability-identifier-naming)
  ValidationSummary validation_summary;                    // NOLINT(readability-identifier-naming)
  std::string version{"0.3"};                              // NOLINT(readability-identifier-naming)
};

// Deterministic JSON serialization.
// Keys are sorted alphabetically (nlohmann::json uses std::map internally).
// Nested arrays are preserved in insertion order; top_rule_ids and evidence_tokens
// must be sorted by the caller before construction if determinism is required.
[[nodiscard]] nlohmann::json decision_record_to_json(const DecisionRecord& record);

// Deserialize a DecisionRecord from JSON. Throws nlohmann::json::exception on
// missing required fields or type mismatches.
[[nodiscard]] DecisionRecord decision_record_from_json(const nlohmann::json& j);

}  // namespace ccmcp::domain
