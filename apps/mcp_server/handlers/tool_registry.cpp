#include "tool_registry.h"

#include "get_audit_trace.h"
#include "get_decision.h"
#include "index_build.h"
#include "ingest_resume.h"
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
      {"ingest_resume", handle_ingest_resume},
      {"index_build", handle_index_build},
      {"get_decision", handle_get_decision},
      {"list_decisions", handle_list_decisions},
  };
}

}  // namespace ccmcp::mcp::handlers
