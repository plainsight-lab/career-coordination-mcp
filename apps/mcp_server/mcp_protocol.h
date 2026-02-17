#pragma once

#include <nlohmann/json.hpp>

#include <optional>
#include <string>

namespace ccmcp::mcp {

// JSON-RPC 2.0 message types
struct JsonRpcRequest {
  std::string jsonrpc{"2.0"};     // NOLINT(readability-identifier-naming)
  std::optional<std::string> id;  // NOLINT(readability-identifier-naming)
  std::string method;             // NOLINT(readability-identifier-naming)
  nlohmann::json params;          // NOLINT(readability-identifier-naming)
};

struct JsonRpcResponse {
  std::string jsonrpc{"2.0"};     // NOLINT(readability-identifier-naming)
  std::optional<std::string> id;  // NOLINT(readability-identifier-naming)
  nlohmann::json result;          // NOLINT(readability-identifier-naming)
};

struct JsonRpcError {
  int code;             // NOLINT(readability-identifier-naming)
  std::string message;  // NOLINT(readability-identifier-naming)
  nlohmann::json data;  // NOLINT(readability-identifier-naming)
};

struct JsonRpcErrorResponse {
  std::string jsonrpc{"2.0"};     // NOLINT(readability-identifier-naming)
  std::optional<std::string> id;  // NOLINT(readability-identifier-naming)
  JsonRpcError error;             // NOLINT(readability-identifier-naming)
};

// Error codes (JSON-RPC 2.0 spec + custom)
constexpr int kParseError = -32700;
constexpr int kInvalidRequest = -32600;
constexpr int kMethodNotFound = -32601;
constexpr int kInvalidParams = -32602;
constexpr int kInternalError = -32603;

// Parse JSON-RPC request from string
std::optional<JsonRpcRequest> parse_request(const std::string& json_str);

// Create JSON-RPC success response
std::string make_response(const std::optional<std::string>& id, const nlohmann::json& result);

// Create JSON-RPC error response
std::string make_error_response(const std::optional<std::string>& id, int code,
                                const std::string& message,
                                const nlohmann::json& data = nlohmann::json::object());

}  // namespace ccmcp::mcp
