#pragma once

#include "ccmcp/matching/matcher.h"

#include <optional>
#include <string>

namespace ccmcp::mcp {

struct McpServerConfig {
  std::optional<std::string> db_path;         // NOLINT(readability-identifier-naming)
  std::optional<std::string> redis_uri;       // NOLINT(readability-identifier-naming)
  std::string vector_backend{"inmemory"};     // NOLINT(readability-identifier-naming)
  matching::MatchingStrategy default_strategy{// NOLINT(readability-identifier-naming)
                                              matching::MatchingStrategy::kDeterministicLexicalV01};
};

McpServerConfig parse_args(int argc, char* argv[]);

}  // namespace ccmcp::mcp
