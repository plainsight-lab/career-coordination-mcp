#include "config.h"

#include "shared/arg_parser.h"
#include <iostream>
#include <string>
#include <vector>

namespace ccmcp::mcp {

namespace {

// ────────────────────────────────────────────────────────────────
// Option Handlers
// ────────────────────────────────────────────────────────────────

bool handle_db(McpServerConfig& config, const std::string& value) {
  config.db_path = value;
  return true;
}

bool handle_redis(McpServerConfig& config, const std::string& value) {
  config.redis_uri = value;
  return true;
}

bool handle_vector_backend(McpServerConfig& config, const std::string& value) {
  if (value == "inmemory" || value == "sqlite") {
    config.vector_backend = value;
    return true;
  }
  std::cerr << "Invalid --vector-backend: " << value << " (valid: inmemory, sqlite)\n";
  return false;
}

bool handle_vector_db_path(McpServerConfig& config, const std::string& value) {
  config.vector_db_path = value;
  return true;
}

bool handle_matching_strategy(McpServerConfig& config, const std::string& value) {
  if (value == "hybrid") {
    config.default_strategy = matching::MatchingStrategy::kHybridLexicalEmbeddingV02;
    return true;
  }
  if (value == "lexical") {
    config.default_strategy = matching::MatchingStrategy::kDeterministicLexicalV01;
    return true;
  }
  std::cerr << "Invalid --matching-strategy: " << value << " (valid: lexical, hybrid)\n";
  return false;
}

// ────────────────────────────────────────────────────────────────
// Option Registry
// ────────────────────────────────────────────────────────────────

std::vector<apps::Option<McpServerConfig>> build_option_registry() {
  return {
      {"--db", true, "Path to SQLite database file", handle_db},
      {"--redis", true, "Redis URI for interaction coordination", handle_redis},
      {"--vector-backend", true, "Vector backend (inmemory|sqlite)", handle_vector_backend},
      {"--vector-db-path", true,
       "Directory for SQLite-backed vector index (required with --vector-backend sqlite)",
       handle_vector_db_path},
      {"--matching-strategy", true, "Matching strategy (lexical|hybrid)", handle_matching_strategy},
  };
}

}  // namespace

// ────────────────────────────────────────────────────────────────
// Parser
// ────────────────────────────────────────────────────────────────

McpServerConfig parse_args(int argc, char* argv[]) {
  return apps::parse_options(argc, argv, build_option_registry());
}

}  // namespace ccmcp::mcp
