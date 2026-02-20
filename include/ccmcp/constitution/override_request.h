#pragma once

#include <nlohmann/json.hpp>

#include <string>

namespace ccmcp::constitution {

// ConstitutionOverrideRequest: an explicit, operator-authorized override for a single
// BLOCK-severity constitutional finding.
//
// Design constraints (per CLAUDE.md §3):
// - Immutable fields after construction — no mutable state.
// - Data members are std::string (not const) so the struct remains copyable per C.12.
// - payload_hash binds this override to a specific artifact using the algorithm
//   identified by binding_hash_alg.  The binding is verified by ValidationEngine::validate()
//   before applying the override.  Override is applied only if both rule_id matches a BLOCK
//   finding AND payload_hash matches.
// - binding_hash_alg: "sha256" (default) or "stable_hash64" (legacy pre-Slice-10 overrides).
// - Serialization: to_json / from_json via override_request_{to,from}_json().
//   Keys are alphabetically sorted by nlohmann::json's std::map default.
struct ConstitutionOverrideRequest {
  std::string binding_hash_alg{"sha256"};  // NOLINT(readability-identifier-naming)
  std::string operator_id;                 // NOLINT(readability-identifier-naming)
  std::string payload_hash;                // NOLINT(readability-identifier-naming)
  std::string reason;                      // NOLINT(readability-identifier-naming)
  std::string rule_id;                     // NOLINT(readability-identifier-naming)
};

// Deterministic JSON serialization (alphabetically sorted keys).
// Declared out-of-line to keep the header free of nlohmann implementation details.
[[nodiscard]] nlohmann::json override_request_to_json(const ConstitutionOverrideRequest& req);

// Deserialize from JSON. Throws nlohmann::json::exception on type mismatch or missing field.
[[nodiscard]] ConstitutionOverrideRequest override_request_from_json(const nlohmann::json& j);

}  // namespace ccmcp::constitution
