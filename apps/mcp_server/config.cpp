#include "config.h"

#include <functional>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace ccmcp::mcp {

namespace {

// ────────────────────────────────────────────────────────────────
// Option Definition
// ────────────────────────────────────────────────────────────────

struct Option {
  std::string name;         // NOLINT(readability-identifier-naming)
  bool requires_value;      // NOLINT(readability-identifier-naming)
  std::string description;  // NOLINT(readability-identifier-naming)
  std::function<bool(McpServerConfig& config, const std::string& value)>
      handler;  // NOLINT(readability-identifier-naming)
};

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

std::vector<Option> build_option_registry() {
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
  McpServerConfig config;
  auto options = build_option_registry();

  // Build lookup map for O(1) option dispatch
  std::unordered_map<std::string, const Option*> option_map;
  for (const auto& opt : options) {
    option_map[opt.name] = &opt;
  }

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];

    auto it = option_map.find(arg);
    if (it != option_map.end()) {
      const Option* opt = it->second;

      if (opt->requires_value) {
        if (i + 1 < argc) {
          std::string value = argv[++i];
          opt->handler(config, value);
        } else {
          std::cerr << "Option " << arg << " requires a value\n";
        }
      } else {
        opt->handler(config, "");
      }
    } else {
      std::cerr << "Unknown option: " << arg << "\n";
    }
  }

  return config;
}

}  // namespace ccmcp::mcp
