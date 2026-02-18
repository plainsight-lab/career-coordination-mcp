#include "method_handlers.h"

#include "handlers/tool_registry.h"

namespace ccmcp::mcp {

using json = nlohmann::json;

json handle_initialize(const JsonRpcRequest& /*req*/, ServerContext& /*ctx*/) {
  return json{
      {"protocolVersion", "2024-11-05"},
      {"capabilities", {{"tools", json::object()}}},
      {"serverInfo", {{"name", "career-coordination-mcp"}, {"version", "0.2.0"}}},
  };
}

json handle_tools_list(const JsonRpcRequest& /*req*/, ServerContext& /*ctx*/) {
  json tools = json::array();

  tools.push_back({
      {"name", "match_opportunity"},
      {"description", "Run matching + validation pipeline for an opportunity"},
      {"inputSchema",
       {
           {"type", "object"},
           {"properties",
            {
                {"opportunity_id", {{"type", "string"}}},
                {"strategy", {{"type", "string"}}},
                {"k_lex", {{"type", "number"}}},
                {"k_emb", {{"type", "number"}}},
                {"trace_id", {{"type", "string"}}},
            }},
           {"required", json::array({"opportunity_id"})},
       }},
  });

  tools.push_back({
      {"name", "validate_match_report"},
      {"description", "Validate a match report (standalone)"},
      {"inputSchema",
       {
           {"type", "object"},
           {"properties", {{"match_report", {{"type", "object"}}}}},
           {"required", json::array({"match_report"})},
       }},
  });

  tools.push_back({
      {"name", "get_audit_trace"},
      {"description", "Fetch audit events by trace_id"},
      {"inputSchema",
       {
           {"type", "object"},
           {"properties", {{"trace_id", {{"type", "string"}}}}},
           {"required", json::array({"trace_id"})},
       }},
  });

  tools.push_back({
      {"name", "interaction_apply_event"},
      {"description", "Apply interaction state transition"},
      {"inputSchema",
       {
           {"type", "object"},
           {"properties",
            {
                {"interaction_id", {{"type", "string"}}},
                {"event", {{"type", "string"}}},
                {"idempotency_key", {{"type", "string"}}},
                {"trace_id", {{"type", "string"}}},
            }},
           {"required", json::array({"interaction_id", "event", "idempotency_key"})},
       }},
  });

  tools.push_back({
      {"name", "ingest_resume"},
      {"description", "Ingest a resume file and optionally persist it to the resume store"},
      {"inputSchema",
       {
           {"type", "object"},
           {"properties",
            {
                {"input_path",
                 {{"type", "string"}, {"description", "Absolute path to resume file"}}},
                {"persist",
                 {{"type", "boolean"}, {"description", "Store the resume (default: true)"}}},
                {"trace_id", {{"type", "string"}}},
            }},
           {"required", json::array({"input_path"})},
       }},
  });

  tools.push_back({
      {"name", "index_build"},
      {"description", "Build or rebuild the embedding vector index for the specified scope"},
      {"inputSchema",
       {
           {"type", "object"},
           {"properties",
            {
                {"scope",
                 {{"type", "string"},
                  {"enum", json::array({"atoms", "resumes", "opps", "all"})},
                  {"description", "Which artifact types to index (default: all)"}}},
                {"trace_id", {{"type", "string"}}},
            }},
       }},
  });

  return json{{"tools", tools}};
}

json handle_tools_call(const JsonRpcRequest& req, ServerContext& ctx) {
  std::string tool_name = req.params.value("name", "");
  json tool_params = req.params.value("arguments", json::object());

  // Tool registry
  static auto tool_registry = handlers::build_tool_registry();

  auto it = tool_registry.find(tool_name);
  if (it == tool_registry.end()) {
    json error_result;
    error_result["error"] = "Unknown tool: " + tool_name;
    return error_result;
  }

  return it->second(tool_params, ctx);
}

std::unordered_map<std::string, MethodHandler> build_method_registry() {
  return {
      {"initialize", handle_initialize},
      {"tools/list", handle_tools_list},
      {"tools/call", handle_tools_call},
  };
}

}  // namespace ccmcp::mcp
