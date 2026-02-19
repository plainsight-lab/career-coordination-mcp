#pragma once

#include "config.h"
#include <string>

namespace ccmcp::mcp {

// validate_mcp_server_config checks startup preconditions for the MCP server.
//
// Returns: "" on success, non-empty error message on failure.
// Caller is responsible for printing the error and exiting with code 1.
//
// Preconditions checked (first failure is returned):
// - redis_uri is present (required — InMemoryInteractionCoordinator is not
//   permitted in production startup paths)
// - if redis_uri is present, parse_redis_uri() must succeed (format valid)
// - if vector_backend == kSqlite, vector_db_path must be present
// - vector_backend != kLanceDb (reserved, not yet implemented)
[[nodiscard]] std::string validate_mcp_server_config(const McpServerConfig& config);

}  // namespace ccmcp::mcp
