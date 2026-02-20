#pragma once

#include <map>
#include <string>

namespace ccmcp::domain {

// RuntimeConfigSnapshot is an immutable record of the process-level configuration
// at startup time.  It is stored once per run before the server loop begins,
// allowing operators to reconstruct the exact runtime environment from stored
// artifacts alone.
//
// schema_version: 7 (introduced in v0.4 Slice 7)
// Keys in the JSON representation are sorted alphabetically for determinism.
struct RuntimeConfigSnapshot {
  int schema_version{7};                             // NOLINT(readability-identifier-naming)
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
