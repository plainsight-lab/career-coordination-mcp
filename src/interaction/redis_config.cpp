#include "ccmcp/interaction/redis_config.h"

#include <string>
#include <string_view>

namespace ccmcp::interaction {

std::optional<RedisConfig> parse_redis_uri(const std::string& uri) {
  if (uri.empty()) {
    return std::nullopt;
  }

  std::string_view view{uri};
  std::string_view host_port_view;

  if (view.starts_with("tcp://")) {
    host_port_view = view.substr(6);
  } else if (view.starts_with("redis://")) {
    host_port_view = view.substr(8);
  } else {
    return std::nullopt;
  }

  if (host_port_view.empty()) {
    return std::nullopt;
  }

  // Split on last colon to separate host from port.
  const auto colon_pos = host_port_view.rfind(':');
  std::string host;
  int port = 6379;

  if (colon_pos == std::string_view::npos) {
    // No port specified — use default.
    host = std::string{host_port_view};
  } else {
    host = std::string{host_port_view.substr(0, colon_pos)};
    const std::string_view port_view = host_port_view.substr(colon_pos + 1);

    if (port_view.empty()) {
      return std::nullopt;
    }

    // Validate: all characters must be decimal digits.
    for (const char c : port_view) {
      if (c < '0' || c > '9') {
        return std::nullopt;
      }
    }

    try {
      port = std::stoi(std::string{port_view});
    } catch (...) {
      return std::nullopt;
    }

    if (port < 1 || port > 65535) {
      return std::nullopt;
    }
  }

  if (host.empty()) {
    return std::nullopt;
  }

  return RedisConfig{uri, host, port};
}

std::string redis_config_to_log_string(const RedisConfig& config) {
  return config.host + ":" + std::to_string(config.port);
}

}  // namespace ccmcp::interaction
