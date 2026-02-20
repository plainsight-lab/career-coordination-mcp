#pragma once

#include "ccmcp/matching/matcher.h"
#include "ccmcp/vector/vector_backend.h"

#include <optional>
#include <string>

namespace ccmcp::mcp {

// AuditChainVerifyMode controls startup-time SHA-256 hash-chain verification.
// kOff  — no verification performed (default; safe for new deployments with no history)
// kWarn — verify all trace chains; emit a warning to stderr for each corrupt trace
// kFail — verify all trace chains; refuse to start if any chain is corrupt
enum class AuditChainVerifyMode {
  kOff,   // NOLINT(readability-identifier-naming)
  kWarn,  // NOLINT(readability-identifier-naming)
  kFail,  // NOLINT(readability-identifier-naming)
};

// McpServerConfig holds all parsed startup flags for the MCP server.
// Every field has an explicit default; optional fields mean "not configured".
struct McpServerConfig {
  std::optional<std::string> db_path;    // NOLINT(readability-identifier-naming)
  std::optional<std::string> redis_uri;  // NOLINT(readability-identifier-naming)
  vector::VectorBackend vector_backend{  // NOLINT(readability-identifier-naming)
                                       vector::VectorBackend::kInMemory};
  // Path for the SQLite-backed vector index; required when vector_backend == kSqlite.
  std::optional<std::string> vector_db_path;  // NOLINT(readability-identifier-naming)
  matching::MatchingStrategy default_strategy{// NOLINT(readability-identifier-naming)
                                              matching::MatchingStrategy::kDeterministicLexicalV01};
  AuditChainVerifyMode audit_chain_verify{// NOLINT(readability-identifier-naming)
                                          AuditChainVerifyMode::kOff};
};

McpServerConfig parse_args(int argc, char* argv[]);  // NOLINT(modernize-avoid-c-arrays)

}  // namespace ccmcp::mcp
