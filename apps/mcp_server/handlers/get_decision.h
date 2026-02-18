#pragma once

#include <nlohmann/json.hpp>

#include "../server_context.h"

namespace ccmcp::mcp::handlers {

nlohmann::json handle_get_decision(const nlohmann::json& params, ServerContext& ctx);
nlohmann::json handle_list_decisions(const nlohmann::json& params, ServerContext& ctx);

}  // namespace ccmcp::mcp::handlers
