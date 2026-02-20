#include "ccmcp/storage/audit_chain.h"

#include "ccmcp/core/sha256.h"

#include <nlohmann/json.hpp>

#include <string>

namespace ccmcp::storage {

std::string compute_event_hash(const AuditEvent& event, const std::string& previous_hash) {
  // Stable JSON serialization: nlohmann::json uses std::map internally so keys are sorted
  // alphabetically, producing deterministic output for identical inputs.
  // Keys: created_at < event_id < event_type < payload < refs < trace_id
  nlohmann::json j;
  j["created_at"] = event.created_at;
  j["event_id"] = event.event_id;
  j["event_type"] = event.event_type;
  j["payload"] = event.payload;
  j["refs"] = event.refs;
  j["trace_id"] = event.trace_id;

  // Concatenate serialized event JSON with previous_hash then hash the result.
  return core::sha256_hex(j.dump() + previous_hash);
}

AuditChainVerificationResult verify_audit_chain(const std::vector<AuditEvent>& events) {
  if (events.empty()) {
    return {true, 0, ""};
  }

  std::string expected_previous = std::string(kGenesisHash);

  for (std::size_t i = 0; i < events.size(); ++i) {
    const AuditEvent& ev = events[i];

    if (ev.previous_hash != expected_previous) {
      return {false, i, "previous_hash mismatch at index " + std::to_string(i)};
    }

    const std::string computed = compute_event_hash(ev, ev.previous_hash);
    if (ev.event_hash != computed) {
      return {false, i, "event_hash mismatch at index " + std::to_string(i)};
    }

    expected_previous = ev.event_hash;
  }

  return {true, events.size(), ""};
}

}  // namespace ccmcp::storage
