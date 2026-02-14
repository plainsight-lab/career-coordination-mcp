#pragma once

#include "ccmcp/storage/audit_event.h"

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
};

}  // namespace ccmcp::storage
