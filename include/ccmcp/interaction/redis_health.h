#pragma once

#include <string>

namespace ccmcp::interaction {

// RedisHealthResult holds the outcome of a redis_ping() call.
struct RedisHealthResult {
  bool reachable{false};  // NOLINT(readability-identifier-naming)
  std::string error;      // NOLINT(readability-identifier-naming)
};

// redis_ping creates a direct Redis connection, sends PING, and returns the result.
//
// This is a state-free operation: PING never mutates Redis data.
// Never throws — all errors are reported in RedisHealthResult.error.
[[nodiscard]] RedisHealthResult redis_ping(const std::string& uri);

}  // namespace ccmcp::interaction
