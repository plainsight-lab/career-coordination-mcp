#include "ccmcp/storage/audit_log.h"

#include "ccmcp/storage/audit_chain.h"

namespace ccmcp::storage {

void InMemoryAuditLog::append(const AuditEvent& event) {
  const auto it = last_hash_.find(event.trace_id);
  const std::string& prev = (it != last_hash_.end()) ? it->second : std::string(kGenesisHash);

  AuditEvent stored = event;
  stored.previous_hash = prev;
  stored.event_hash = compute_event_hash(event, prev);

  last_hash_[event.trace_id] = stored.event_hash;
  events_.push_back(std::move(stored));
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

std::vector<std::string> InMemoryAuditLog::list_trace_ids() const {
  std::vector<std::string> ids;
  ids.reserve(last_hash_.size());
  for (const auto& [k, _] : last_hash_) {
    ids.push_back(k);
  }
  return ids;
}

}  // namespace ccmcp::storage
