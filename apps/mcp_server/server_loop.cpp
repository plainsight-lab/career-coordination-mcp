#include "server_loop.h"

#include <nlohmann/json.hpp>

#include "mcp_protocol.h"
#include "method_handlers.h"
#include <iostream>
#include <string>

namespace ccmcp::mcp {

using json = nlohmann::json;

void run_server_loop(ServerContext& ctx) {
  // Method registry
  auto method_registry = build_method_registry();

  // Main loop: read JSON-RPC requests from stdin, write responses to stdout
  std::string line;
  while (std::getline(std::cin, line)) {
    if (line.empty()) {
      continue;
    }

    auto request_opt = parse_request(line);
    if (!request_opt.has_value()) {
      std::cout << make_error_response(std::nullopt, kParseError, "Invalid JSON") << "\n"
                << std::flush;
      continue;
    }

    auto request = request_opt.value();
    std::cerr << "Received: " << request.method << "\n";

    // Dispatch via method registry
    auto it = method_registry.find(request.method);
    if (it != method_registry.end()) {
      json result = it->second(request, ctx);
      std::cout << make_response(request.id, result) << "\n" << std::flush;
    } else {
      std::cout << make_error_response(request.id, kMethodNotFound,
                                       "Unknown method: " + request.method)
                << "\n"
                << std::flush;
    }
  }

  std::cerr << "MCP Server shutting down\n";
}

}  // namespace ccmcp::mcp
