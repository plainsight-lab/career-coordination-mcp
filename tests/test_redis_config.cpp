#include "ccmcp/interaction/redis_config.h"

#include <catch2/catch_test_macros.hpp>

using namespace ccmcp::interaction;

// ── parse_redis_uri: accepted formats ──────────────────────────────────────

TEST_CASE("parse_redis_uri: tcp://host:port", "[redis][config]") {
  const auto result = parse_redis_uri("tcp://127.0.0.1:6379");
  REQUIRE(result.has_value());
  CHECK(result->host == "127.0.0.1");
  CHECK(result->port == 6379);
  CHECK(result->uri == "tcp://127.0.0.1:6379");
}

TEST_CASE("parse_redis_uri: redis://host:port", "[redis][config]") {
  const auto result = parse_redis_uri("redis://localhost:6379");
  REQUIRE(result.has_value());
  CHECK(result->host == "localhost");
  CHECK(result->port == 6379);
  CHECK(result->uri == "redis://localhost:6379");
}

TEST_CASE("parse_redis_uri: tcp://host without port defaults to 6379", "[redis][config]") {
  const auto result = parse_redis_uri("tcp://myhost");
  REQUIRE(result.has_value());
  CHECK(result->host == "myhost");
  CHECK(result->port == 6379);
}

// ── parse_redis_uri: rejected formats ──────────────────────────────────────

TEST_CASE("parse_redis_uri: empty string returns nullopt", "[redis][config]") {
  CHECK_FALSE(parse_redis_uri("").has_value());
}

TEST_CASE("parse_redis_uri: unrecognised or missing scheme returns nullopt", "[redis][config]") {
  CHECK_FALSE(parse_redis_uri("not-a-uri").has_value());
  CHECK_FALSE(parse_redis_uri("http://localhost:6379").has_value());
  CHECK_FALSE(parse_redis_uri("://localhost:6379").has_value());
  CHECK_FALSE(parse_redis_uri("localhost:6379").has_value());
}

// ── parse_redis_uri: redis_db parsing ───────────────────────────────────────

TEST_CASE("parse_redis_uri: redis://host:port/N sets redis_db", "[redis][config]") {
  const auto result = parse_redis_uri("redis://localhost:6379/1");
  REQUIRE(result.has_value());
  CHECK(result->host == "localhost");
  CHECK(result->port == 6379);
  CHECK(result->redis_db == 1);
  CHECK(result->uri == "redis://localhost:6379/1");
}

TEST_CASE("parse_redis_uri: redis://host:port without /N has redis_db=0", "[redis][config]") {
  const auto result = parse_redis_uri("redis://localhost:6379");
  REQUIRE(result.has_value());
  CHECK(result->redis_db == 0);
}

// ── redis_config_to_log_string: determinism ─────────────────────────────────

TEST_CASE("redis_config_to_log_string: deterministic — same input produces same output",
          "[redis][config]") {
  const RedisConfig config{"tcp://127.0.0.1:6379", "127.0.0.1", 6379};
  const std::string s1 = redis_config_to_log_string(config);
  const std::string s2 = redis_config_to_log_string(config);
  CHECK(s1 == s2);
  CHECK(s1 == "127.0.0.1:6379");
}

TEST_CASE("redis_config_to_log_string: non-default port is included", "[redis][config]") {
  const RedisConfig config{"redis://myhost:1234", "myhost", 1234};
  CHECK(redis_config_to_log_string(config) == "myhost:1234");
}
