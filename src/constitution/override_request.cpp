#include "ccmcp/constitution/override_request.h"

namespace ccmcp::constitution {

nlohmann::json override_request_to_json(const ConstitutionOverrideRequest& req) {
  // nlohmann::json uses std::map internally by default, so keys are alphabetically sorted.
  // Field order: operator_id, payload_hash, reason, rule_id (alphabetical).
  nlohmann::json j;
  j["operator_id"] = req.operator_id;
  j["payload_hash"] = req.payload_hash;
  j["reason"] = req.reason;
  j["rule_id"] = req.rule_id;
  return j;
}

ConstitutionOverrideRequest override_request_from_json(const nlohmann::json& j) {
  ConstitutionOverrideRequest req;
  req.operator_id = j.at("operator_id").get<std::string>();
  req.payload_hash = j.at("payload_hash").get<std::string>();
  req.reason = j.at("reason").get<std::string>();
  req.rule_id = j.at("rule_id").get<std::string>();
  return req;
}

}  // namespace ccmcp::constitution
