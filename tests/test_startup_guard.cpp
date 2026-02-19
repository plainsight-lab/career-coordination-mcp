#include <catch2/catch_test_macros.hpp>

#include "config.h"
#include "startup_guard.h"

using namespace ccmcp::mcp;
using ccmcp::vector::VectorBackend;

// ── Test A: Redis required ──────────────────────────────────────────────────

TEST_CASE("validate_mcp_server_config: no redis_uri returns error", "[startup][config]") {
  McpServerConfig config;
  config.redis_uri = std::nullopt;
  config.vector_backend = VectorBackend::kInMemory;

  const auto error = validate_mcp_server_config(config);
  CHECK_FALSE(error.empty());
}

// ── Test B: Valid configuration ─────────────────────────────────────────────

TEST_CASE("validate_mcp_server_config: valid redis + kInMemory vector returns empty",
          "[startup][config]") {
  McpServerConfig config;
  config.redis_uri = "tcp://127.0.0.1:6379";
  config.vector_backend = VectorBackend::kInMemory;

  const auto error = validate_mcp_server_config(config);
  CHECK(error.empty());
}

TEST_CASE("validate_mcp_server_config: valid redis + kSqlite with path returns empty",
          "[startup][config]") {
  McpServerConfig config;
  config.redis_uri = "tcp://127.0.0.1:6379";
  config.vector_backend = VectorBackend::kSqlite;
  config.vector_db_path = "/tmp/vectors";

  const auto error = validate_mcp_server_config(config);
  CHECK(error.empty());
}

// ── Test C: No fallback — invalid URI format is rejected ────────────────────

TEST_CASE("validate_mcp_server_config: invalid redis URI format returns error",
          "[startup][config]") {
  McpServerConfig config;
  config.redis_uri = "not-a-valid-uri";
  config.vector_backend = VectorBackend::kInMemory;

  const auto error = validate_mcp_server_config(config);
  CHECK_FALSE(error.empty());
}

// ── kSqlite path constraint ─────────────────────────────────────────────────

TEST_CASE("validate_mcp_server_config: valid redis + kSqlite without path returns error",
          "[startup][config]") {
  McpServerConfig config;
  config.redis_uri = "tcp://127.0.0.1:6379";
  config.vector_backend = VectorBackend::kSqlite;
  config.vector_db_path = std::nullopt;

  const auto error = validate_mcp_server_config(config);
  CHECK_FALSE(error.empty());
}

// ── kLanceDb constraint (pre-existing) ─────────────────────────────────────

TEST_CASE("validate_mcp_server_config: kLanceDb returns error", "[startup][config]") {
  McpServerConfig config;
  config.redis_uri = "tcp://127.0.0.1:6379";
  config.vector_backend = VectorBackend::kLanceDb;

  const auto error = validate_mcp_server_config(config);
  CHECK_FALSE(error.empty());
}
