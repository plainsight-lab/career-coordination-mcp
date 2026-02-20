#pragma once

#include "ccmcp/storage/audit_event.h"

#include <map>
#include <string>
#include <vector>

namespace ccmcp::storage {

class IAuditLog {
 public:
  virtual ~IAuditLog() = default;
  virtual void append(const AuditEvent& event) = 0;
  virtual std::vector<AuditEvent> query(const std::string& trace_id) const = 0;
};

class InMemoryAuditLog final : public IAuditLog {
 public:
  void append(const AuditEvent& event) override;
  [[nodiscard]] std::vector<AuditEvent> query(const std::string& trace_id) const override;

 private:
  std::vector<AuditEvent> events_;
  // Per-trace last event_hash, used to chain new events.
  std::map<std::string, std::string> last_hash_;
};

}  // namespace ccmcp::storage
