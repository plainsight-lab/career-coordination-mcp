#include "tool_registry.h"

#include "get_audit_trace.h"
#include "interaction_apply_event.h"
#include "match_opportunity.h"
#include "validate_match_report.h"

namespace ccmcp::mcp::handlers {

std::unordered_map<std::string, ToolHandler> build_tool_registry() {
  return {
      {"match_opportunity", handle_match_opportunity},
      {"validate_match_report", handle_validate_match_report},
      {"get_audit_trace", handle_get_audit_trace},
      {"interaction_apply_event", handle_interaction_apply_event},
  };
}

}  // namespace ccmcp::mcp::handlers
