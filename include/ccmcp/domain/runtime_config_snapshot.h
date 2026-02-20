#pragma once

#include <map>
#include <string>

namespace ccmcp::domain {

// RuntimeConfigSnapshot is an immutable record of the process-level configuration
// at startup time.  It is stored once per run before the server loop begins,
// allowing operators to reconstruct the exact runtime environment from stored
// artifacts alone.
//
// snapshot_format_version: tracks the JSON schema of this struct itself.
//   v1 = Slice 7 original (serialized key: "schema_version")
//   v2 = Slice 10+ (serialized key: "snapshot_format_version"; adds db_schema_version)
// db_schema_version: the applied SQLite schema version at startup (e.g. 8 after ensure_schema_v8).
// Keys in the JSON representation are sorted alphabetically for determinism.
struct RuntimeConfigSnapshot {
  int snapshot_format_version{2};                    // NOLINT(readability-identifier-naming)
  int db_schema_version{0};                          // NOLINT(readability-identifier-naming)
  std::string vector_backend;                        // NOLINT(readability-identifier-naming)
  std::string redis_host;                            // NOLINT(readability-identifier-naming)
  int redis_port{6379};                              // NOLINT(readability-identifier-naming)
  int redis_db{0};                                   // NOLINT(readability-identifier-naming)
  std::string build_version;                         // NOLINT(readability-identifier-naming)
  std::map<std::string, std::string> feature_flags;  // NOLINT(readability-identifier-naming)
};

// to_json serializes a RuntimeConfigSnapshot to a JSON string.
// Keys are sorted alphabetically.  Output is deterministic given the same input.
[[nodiscard]] std::string to_json(const RuntimeConfigSnapshot& snapshot);

// from_json deserializes a RuntimeConfigSnapshot from a JSON string.
// Throws std::runtime_error if required fields are absent or have wrong types.
[[nodiscard]] RuntimeConfigSnapshot from_json(const std::string& json_str);

}  // namespace ccmcp::domain
