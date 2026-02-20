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
  // Returns the distinct trace IDs stored in this log.
  // Used at startup to enumerate traces for hash-chain verification.
  [[nodiscard]] virtual std::vector<std::string> list_trace_ids() const = 0;
};

class InMemoryAuditLog final : public IAuditLog {
 public:
  void append(const AuditEvent& event) override;
  [[nodiscard]] std::vector<AuditEvent> query(const std::string& trace_id) const override;
  [[nodiscard]] std::vector<std::string> list_trace_ids() const override;

 private:
  std::vector<AuditEvent> events_;
  // Per-trace last event_hash, used to chain new events.
  // Keys are exactly the distinct trace IDs present in this log.
  std::map<std::string, std::string> last_hash_;
};

}  // namespace ccmcp::storage
