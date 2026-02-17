#include "get_audit_trace.h"

#include "ccmcp/app/app_service.h"

#include <exception>
#include <string>

namespace ccmcp::mcp::handlers {

using json = nlohmann::json;

json handle_get_audit_trace(const json& params, ServerContext& ctx) {
  try {
    std::string trace_id = params.at("trace_id");
    auto events = app::fetch_audit_trace(trace_id, ctx.services);

    json result;
    result["trace_id"] = trace_id;
    result["events"] = json::array();

    for (const auto& event : events) {
      result["events"].push_back({
          {"event_id", event.event_id},
          {"trace_id", event.trace_id},
          {"event_type", event.event_type},
          {"payload", json::parse(event.payload)},
          {"created_at", event.created_at},
      });
    }

    return result;

  } catch (const std::exception& e) {
    json error_result;
    error_result["error"] = e.what();
    return error_result;
  }
}

}  // namespace ccmcp::mcp::handlers
