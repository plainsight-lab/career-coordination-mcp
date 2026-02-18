#include "get_decision.h"

#include "ccmcp/app/app_service.h"
#include "ccmcp/domain/decision_record.h"

#include <exception>
#include <string>

namespace ccmcp::mcp::handlers {

using json = nlohmann::json;

json handle_get_decision(const json& params, ServerContext& ctx) {
  try {
    const std::string decision_id = params.at("decision_id");
    auto record = app::fetch_decision(decision_id, ctx.decision_store);

    if (!record.has_value()) {
      json error_result;
      error_result["error"] = "Decision not found: " + decision_id;
      return error_result;
    }

    return domain::decision_record_to_json(record.value());

  } catch (const std::exception& e) {
    json error_result;
    error_result["error"] = e.what();
    return error_result;
  }
}

json handle_list_decisions(const json& params, ServerContext& ctx) {
  try {
    const std::string trace_id = params.at("trace_id");
    auto records = app::list_decisions_by_trace(trace_id, ctx.decision_store);

    json result;
    result["trace_id"] = trace_id;
    result["decisions"] = json::array();
    for (const auto& record : records) {
      result["decisions"].push_back(domain::decision_record_to_json(record));
    }

    return result;

  } catch (const std::exception& e) {
    json error_result;
    error_result["error"] = e.what();
    return error_result;
  }
}

}  // namespace ccmcp::mcp::handlers
