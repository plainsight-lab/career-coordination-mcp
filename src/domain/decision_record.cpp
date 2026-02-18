#include "ccmcp/domain/decision_record.h"

namespace ccmcp::domain {

nlohmann::json decision_record_to_json(const DecisionRecord& record) {
  using json = nlohmann::json;

  // Build requirement_decisions array
  json req_decisions = json::array();
  for (const auto& rd : record.requirement_decisions) {
    json entry;
    entry["requirement_text"] = rd.requirement_text;
    if (rd.atom_id.has_value()) {
      entry["atom_id"] = rd.atom_id.value();
    } else {
      entry["atom_id"] = nullptr;
    }
    entry["evidence_tokens"] = rd.evidence_tokens;
    req_decisions.push_back(std::move(entry));
  }

  // Build retrieval_stats
  json stats;
  stats["embedding_candidates"] = record.retrieval_stats.embedding_candidates;
  stats["lexical_candidates"] = record.retrieval_stats.lexical_candidates;
  stats["merged_candidates"] = record.retrieval_stats.merged_candidates;

  // Build validation_summary
  json vsummary;
  vsummary["fail_count"] = record.validation_summary.fail_count;
  vsummary["finding_count"] = record.validation_summary.finding_count;
  vsummary["status"] = record.validation_summary.status;
  vsummary["top_rule_ids"] = record.validation_summary.top_rule_ids;
  vsummary["warn_count"] = record.validation_summary.warn_count;

  // Top-level — nlohmann::json default object type is std::map, so keys sort alphabetically.
  json j;
  j["artifact_id"] = record.artifact_id;
  if (record.created_at.has_value()) {
    j["created_at"] = record.created_at.value();
  } else {
    j["created_at"] = nullptr;
  }
  j["decision_id"] = record.decision_id;
  j["opportunity_id"] = record.opportunity_id;
  j["requirement_decisions"] = std::move(req_decisions);
  j["retrieval_stats"] = std::move(stats);
  j["trace_id"] = record.trace_id;
  j["validation_summary"] = std::move(vsummary);
  j["version"] = record.version;

  return j;
}

DecisionRecord decision_record_from_json(const nlohmann::json& j) {
  DecisionRecord record;
  record.decision_id = j.at("decision_id").get<std::string>();
  record.trace_id = j.at("trace_id").get<std::string>();
  record.artifact_id = j.at("artifact_id").get<std::string>();
  record.opportunity_id = j.at("opportunity_id").get<std::string>();
  record.version = j.at("version").get<std::string>();

  if (j.at("created_at").is_null()) {
    record.created_at = std::nullopt;
  } else {
    record.created_at = j.at("created_at").get<std::string>();
  }

  for (const auto& rd_json : j.at("requirement_decisions")) {
    RequirementDecision rd;
    rd.requirement_text = rd_json.at("requirement_text").get<std::string>();
    if (rd_json.at("atom_id").is_null()) {
      rd.atom_id = std::nullopt;
    } else {
      rd.atom_id = rd_json.at("atom_id").get<std::string>();
    }
    rd.evidence_tokens = rd_json.at("evidence_tokens").get<std::vector<std::string>>();
    record.requirement_decisions.push_back(std::move(rd));
  }

  const auto& stats = j.at("retrieval_stats");
  record.retrieval_stats.lexical_candidates = stats.at("lexical_candidates").get<size_t>();
  record.retrieval_stats.embedding_candidates = stats.at("embedding_candidates").get<size_t>();
  record.retrieval_stats.merged_candidates = stats.at("merged_candidates").get<size_t>();

  const auto& vs = j.at("validation_summary");
  record.validation_summary.status = vs.at("status").get<std::string>();
  record.validation_summary.finding_count = vs.at("finding_count").get<size_t>();
  record.validation_summary.fail_count = vs.at("fail_count").get<size_t>();
  record.validation_summary.warn_count = vs.at("warn_count").get<size_t>();
  record.validation_summary.top_rule_ids = vs.at("top_rule_ids").get<std::vector<std::string>>();

  return record;
}

}  // namespace ccmcp::domain
