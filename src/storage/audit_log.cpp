#include "ccmcp/storage/audit_log.h"

namespace ccmcp::storage {

void InMemoryAuditLog::append(const AuditEvent& event) {
  // Append-only in-memory store; deterministic persistence backend will replace this.
  events_.push_back(event);
}

std::vector<AuditEvent> InMemoryAuditLog::query(const std::string& trace_id) const {
  if (trace_id.empty()) {
    return events_;
  }

  std::vector<AuditEvent> filtered;
  filtered.reserve(events_.size());
  for (const auto& event : events_) {
    if (event.trace_id == trace_id) {
      filtered.push_back(event);
    }
  }
  return filtered;
}

}  // namespace ccmcp::storage
