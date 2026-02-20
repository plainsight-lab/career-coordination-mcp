#include "ccmcp/domain/runtime_config_snapshot.h"

#include <nlohmann/json.hpp>

#include <stdexcept>

namespace ccmcp::domain {

std::string to_json(const RuntimeConfigSnapshot& snapshot) {
  using json = nlohmann::json;

  // Top-level object — nlohmann::json default container is std::map, so keys sort alphabetically.
  json j;
  j["build_version"] = snapshot.build_version;
  j["feature_flags"] = snapshot.feature_flags;
  j["redis_db"] = snapshot.redis_db;
  j["redis_host"] = snapshot.redis_host;
  j["redis_port"] = snapshot.redis_port;
  j["schema_version"] = snapshot.schema_version;
  j["vector_backend"] = snapshot.vector_backend;

  return j.dump();
}

RuntimeConfigSnapshot from_json(const std::string& json_str) {
  using json = nlohmann::json;

  const json j = json::parse(json_str);

  RuntimeConfigSnapshot snapshot;
  snapshot.schema_version = j.at("schema_version").get<int>();
  snapshot.vector_backend = j.at("vector_backend").get<std::string>();
  snapshot.redis_host = j.at("redis_host").get<std::string>();
  snapshot.redis_port = j.at("redis_port").get<int>();
  snapshot.redis_db = j.at("redis_db").get<int>();
  snapshot.build_version = j.at("build_version").get<std::string>();
  snapshot.feature_flags = j.at("feature_flags").get<std::map<std::string, std::string>>();

  return snapshot;
}

}  // namespace ccmcp::domain
