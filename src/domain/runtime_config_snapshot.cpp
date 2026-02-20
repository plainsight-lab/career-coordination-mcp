#include "ccmcp/domain/runtime_config_snapshot.h"

#include <nlohmann/json.hpp>

#include <stdexcept>

namespace ccmcp::domain {

std::string to_json(const RuntimeConfigSnapshot& snapshot) {
  using json = nlohmann::json;

  // Top-level object — nlohmann::json default container is std::map, so keys sort alphabetically.
  // Key ordering: build_version < db_schema_version < feature_flags < redis_db < redis_host
  //               < redis_port < snapshot_format_version < vector_backend
  json j;
  j["build_version"] = snapshot.build_version;
  j["db_schema_version"] = snapshot.db_schema_version;
  j["feature_flags"] = snapshot.feature_flags;
  j["redis_db"] = snapshot.redis_db;
  j["redis_host"] = snapshot.redis_host;
  j["redis_port"] = snapshot.redis_port;
  j["snapshot_format_version"] = snapshot.snapshot_format_version;
  j["vector_backend"] = snapshot.vector_backend;

  return j.dump();
}

RuntimeConfigSnapshot from_json(const std::string& json_str) {
  using json = nlohmann::json;

  const json j = json::parse(json_str);

  RuntimeConfigSnapshot snapshot;

  // Legacy compat: Slice 7 snapshots used "schema_version"; Slice 10+ use
  // "snapshot_format_version".
  if (j.contains("snapshot_format_version")) {
    snapshot.snapshot_format_version = j.at("snapshot_format_version").get<int>();
  } else if (j.contains("schema_version")) {
    snapshot.snapshot_format_version = j.at("schema_version").get<int>();  // legacy v1
  }
  snapshot.db_schema_version = j.value("db_schema_version", 0);

  snapshot.vector_backend = j.at("vector_backend").get<std::string>();
  snapshot.redis_host = j.at("redis_host").get<std::string>();
  snapshot.redis_port = j.at("redis_port").get<int>();
  snapshot.redis_db = j.at("redis_db").get<int>();
  snapshot.build_version = j.at("build_version").get<std::string>();
  snapshot.feature_flags = j.at("feature_flags").get<std::map<std::string, std::string>>();

  return snapshot;
}

}  // namespace ccmcp::domain
