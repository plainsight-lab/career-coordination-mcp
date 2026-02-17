#pragma once

#include <nlohmann/json.hpp>

#include "../server_context.h"
#include <functional>
#include <string>
#include <unordered_map>

namespace ccmcp::mcp::handlers {

using ToolHandler = std::function<nlohmann::json(const nlohmann::json& params, ServerContext& ctx)>;

std::unordered_map<std::string, ToolHandler> build_tool_registry();

}  // namespace ccmcp::mcp::handlers
