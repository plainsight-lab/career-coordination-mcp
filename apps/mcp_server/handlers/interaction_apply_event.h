#pragma once

#include <nlohmann/json.hpp>

#include "../server_context.h"

namespace ccmcp::mcp::handlers {

nlohmann::json handle_interaction_apply_event(const nlohmann::json& params, ServerContext& ctx);

}  // namespace ccmcp::mcp::handlers
