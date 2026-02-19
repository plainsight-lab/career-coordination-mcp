#include "startup_guard.h"

#include "ccmcp/interaction/redis_config.h"
#include "ccmcp/vector/vector_backend.h"

namespace ccmcp::mcp {

std::string validate_mcp_server_config(const McpServerConfig& config) {
  // Redis is required: production startup must have an explicit coordinator URI.
  // InMemoryInteractionCoordinator is not permitted in production startup paths.
  if (!config.redis_uri.has_value()) {
    return "Error: --redis <uri> is required.\n"
           "       The MCP server requires Redis for durable interaction coordination.\n"
           "       Pass --redis tcp://host:port to enable it.\n"
           "       See docs/DEVELOPMENT.md for local Redis setup.";
  }

  // Validate Redis URI format before attempting to connect.
  if (!interaction::parse_redis_uri(config.redis_uri.value()).has_value()) {
    return "Error: --redis URI '" + config.redis_uri.value() +
           "' is not a valid Redis URI.\n"
           "       Accepted formats: tcp://host:port, redis://host:port, tcp://host";
  }

  // Vector backend constraints.
  switch (config.vector_backend) {
    case vector::VectorBackend::kSqlite:
      if (!config.vector_db_path.has_value()) {
        return "Error: --vector-db-path <dir> is required when --vector-backend sqlite";
      }
      break;
    case vector::VectorBackend::kLanceDb:
      return "Error: --vector-backend lancedb is reserved and not yet implemented.\n"
             "       Use --vector-backend sqlite for persistent vector storage.";
    case vector::VectorBackend::kInMemory:
      break;
  }

  return "";
}

}  // namespace ccmcp::mcp
