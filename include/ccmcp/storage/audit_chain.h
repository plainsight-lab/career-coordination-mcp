#pragma once

#include "ccmcp/storage/audit_event.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace ccmcp::storage {

// Genesis hash used as previous_hash for the first event in each trace.
// 64 zero hex digits — clearly synthetic, deterministic, documents intent.
inline constexpr std::string_view kGenesisHash =
    "0000000000000000000000000000000000000000000000000000000000000000";

// Compute the SHA-256 hash for an audit event.
//
// Input:  stable JSON of event fields (excluding hash fields, keys sorted alphabetically)
//         concatenated with previous_hash.
// Output: 64-character lowercase hex string (FIPS 180-4).
//
// This function is pure: given the same event and previous_hash it always returns the same digest.
[[nodiscard]] std::string compute_event_hash(const AuditEvent& event,
                                             const std::string& previous_hash);

// Result of verifying an audit hash chain.
struct AuditChainVerificationResult {
  bool valid{false};                  // NOLINT(readability-identifier-naming)
  std::size_t first_invalid_index{};  // NOLINT(readability-identifier-naming)
  std::string error;                  // NOLINT(readability-identifier-naming)
};

// Verify that a sequence of audit events forms a valid SHA-256 hash chain.
//
// Events must be in append order (as returned by IAuditLog::query with ORDER BY idx).
// The first event is expected to have previous_hash == kGenesisHash.
//
// Returns:
//   valid == true,  first_invalid_index == events.size()  — chain intact
//   valid == false, first_invalid_index == N              — event N is corrupt or out of order
[[nodiscard]] AuditChainVerificationResult verify_audit_chain(
    const std::vector<AuditEvent>& events);

}  // namespace ccmcp::storage
