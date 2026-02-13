#pragma once

#include <string>
#include <vector>

namespace ccmcp::storage {

struct AuditEvent {
  std::string event_id;
  std::string trace_id;
  std::string event_type;
  std::string payload;
  std::string created_at;
  std::vector<std::string> refs;
};

}  // namespace ccmcp::storage
