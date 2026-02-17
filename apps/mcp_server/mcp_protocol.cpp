#include "mcp_protocol.h"

namespace ccmcp::mcp {

std::optional<JsonRpcRequest> parse_request(const std::string& json_str) {
  try {
    auto json = nlohmann::json::parse(json_str);

    JsonRpcRequest request;
    request.jsonrpc = json.value("jsonrpc", "2.0");

    if (json.contains("id")) {
      if (json["id"].is_string()) {
        request.id = json["id"].get<std::string>();
      } else if (json["id"].is_number()) {
        request.id = std::to_string(json["id"].get<int>());
      }
    }

    request.method = json.value("method", "");
    request.params = json.value("params", nlohmann::json::object());

    return request;
  } catch (const std::exception&) {
    return std::nullopt;
  }
}

std::string make_response(const std::optional<std::string>& id, const nlohmann::json& result) {
  nlohmann::json response;
  response["jsonrpc"] = "2.0";
  if (id.has_value()) {
    response["id"] = id.value();
  } else {
    response["id"] = nullptr;
  }
  response["result"] = result;
  return response.dump();
}

std::string make_error_response(const std::optional<std::string>& id, int code,
                                const std::string& message, const nlohmann::json& data) {
  nlohmann::json response;
  response["jsonrpc"] = "2.0";
  if (id.has_value()) {
    response["id"] = id.value();
  } else {
    response["id"] = nullptr;
  }
  response["error"] = {
      {"code", code},
      {"message", message},
      {"data", data},
  };
  return response.dump();
}

}  // namespace ccmcp::mcp
