#pragma once

#include "ccmcp/matching/matcher.h"

#include <optional>
#include <string>

namespace ccmcp::mcp {

// McpServerConfig holds all parsed startup flags for the MCP server.
// Every field has an explicit default; optional fields mean "not configured".
struct McpServerConfig {
  std::optional<std::string> db_path;      // NOLINT(readability-identifier-naming)
  std::optional<std::string> redis_uri;    // NOLINT(readability-identifier-naming)
  std::string vector_backend{"inmemory"};  // NOLINT(readability-identifier-naming)
  // Path for the lancedb (SQLite-backed) vector index; required when vector_backend == "lancedb"
  std::optional<std::string> lancedb_path;    // NOLINT(readability-identifier-naming)
  matching::MatchingStrategy default_strategy{// NOLINT(readability-identifier-naming)
                                              matching::MatchingStrategy::kDeterministicLexicalV01};
};

McpServerConfig parse_args(int argc, char* argv[]);  // NOLINT(modernize-avoid-c-arrays)

}  // namespace ccmcp::mcp
