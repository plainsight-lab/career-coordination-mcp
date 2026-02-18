#pragma once

#include <nlohmann/json.hpp>

#include "../server_context.h"

namespace ccmcp::mcp::handlers {

nlohmann::json handle_index_build(const nlohmann::json& params, ServerContext& ctx);

}  // namespace ccmcp::mcp::handlers
