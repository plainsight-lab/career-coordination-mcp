#pragma once

#include <nlohmann/json.hpp>

#include "mcp_protocol.h"
#include "server_context.h"
#include <functional>
#include <string>
#include <unordered_map>

namespace ccmcp::mcp {

using MethodHandler = std::function<nlohmann::json(const JsonRpcRequest& req, ServerContext& ctx)>;

nlohmann::json handle_initialize(const JsonRpcRequest& req, ServerContext& ctx);
nlohmann::json handle_tools_list(const JsonRpcRequest& req, ServerContext& ctx);
nlohmann::json handle_tools_call(const JsonRpcRequest& req, ServerContext& ctx);

std::unordered_map<std::string, MethodHandler> build_method_registry();

}  // namespace ccmcp::mcp
