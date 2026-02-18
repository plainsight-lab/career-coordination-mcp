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
  if (value == "inmemory" || value == "lancedb") {
    config.vector_backend = value;
    return true;
  }
  std::cerr << "Invalid --vector-backend: " << value << " (valid: inmemory, lancedb)\n";
  return false;
}

bool handle_lancedb_path(McpServerConfig& config, const std::string& value) {
  config.lancedb_path = value;
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
      {"--vector-backend", true, "Vector backend (inmemory|lancedb)", handle_vector_backend},
      {"--lancedb-path", true,
       "Directory for LanceDB vector index (required with --vector-backend lancedb)",
       handle_lancedb_path},
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
