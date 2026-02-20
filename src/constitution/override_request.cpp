#include "ccmcp/constitution/override_request.h"

namespace ccmcp::constitution {

nlohmann::json override_request_to_json(const ConstitutionOverrideRequest& req) {
  // nlohmann::json uses std::map internally by default, so keys are alphabetically sorted.
  // Field order: binding_hash_alg, operator_id, payload_hash, reason, rule_id (alphabetical).
  nlohmann::json j;
  j["binding_hash_alg"] = req.binding_hash_alg;
  j["operator_id"] = req.operator_id;
  j["payload_hash"] = req.payload_hash;
  j["reason"] = req.reason;
  j["rule_id"] = req.rule_id;
  return j;
}

ConstitutionOverrideRequest override_request_from_json(const nlohmann::json& j) {
  ConstitutionOverrideRequest req;
  // binding_hash_alg is optional for backward-compat: old overrides without this field
  // default to "sha256" (the current algorithm, matching the new default in the struct).
  req.binding_hash_alg = j.value("binding_hash_alg", std::string{"sha256"});
  req.operator_id = j.at("operator_id").get<std::string>();
  req.payload_hash = j.at("payload_hash").get<std::string>();
  req.reason = j.at("reason").get<std::string>();
  req.rule_id = j.at("rule_id").get<std::string>();
  return req;
}

}  // namespace ccmcp::constitution
