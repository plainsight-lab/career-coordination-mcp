#pragma once

#include <optional>
#include <string>

namespace ccmcp::interaction {

// RedisConfig holds a parsed and validated Redis URI.
//
// URI formats accepted:
//   tcp://host:port
//   redis://host:port
//   tcp://host          (port defaults to 6379)
//   redis://host:port/N (N = database index, redis:// scheme only)
struct RedisConfig {
  std::string uri;   // NOLINT(readability-identifier-naming)
  std::string host;  // NOLINT(readability-identifier-naming)
  int port{6379};    // NOLINT(readability-identifier-naming)
  int redis_db{0};   // NOLINT(readability-identifier-naming)
};

// parse_redis_uri attempts to parse a Redis URI string.
// Returns RedisConfig on success, nullopt if the format is not recognised.
//
// Accepts: tcp://host:port, redis://host:port, tcp://host, redis://host:port/N
// Rejects: empty string, no recognised scheme, missing host, invalid port
//
// For redis:// URIs, an optional /N path component sets the database index.
// The database index defaults to 0 when absent.
// For tcp:// URIs, redis_db is always 0.
//
// No dependency on redis++ — pure string parsing.
[[nodiscard]] std::optional<RedisConfig> parse_redis_uri(const std::string& uri);

// redis_config_to_log_string returns a deterministic, human-readable
// representation of a RedisConfig for startup diagnostics.
// Format: "host:port"
[[nodiscard]] std::string redis_config_to_log_string(const RedisConfig& config);

}  // namespace ccmcp::interaction
