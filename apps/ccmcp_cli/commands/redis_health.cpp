#include "redis_health.h"

#include "ccmcp/interaction/redis_config.h"
#include "ccmcp/interaction/redis_health.h"

#include "shared/arg_parser.h"
#include <iostream>
#include <optional>
#include <string>
#include <vector>

namespace {

struct RedisHealthCliConfig {
  std::optional<std::string> redis_uri;  // NOLINT(readability-identifier-naming)
};

}  // namespace

int cmd_redis_health(int argc, char* argv[]) {  // NOLINT(modernize-avoid-c-arrays)
  const std::vector<ccmcp::apps::Option<RedisHealthCliConfig>> options = {
      {"--redis", true, "Redis URI (e.g. tcp://127.0.0.1:6379)",
       [](RedisHealthCliConfig& c, const std::string& v) {
         c.redis_uri = v;
         return true;
       }},
  };
  auto config = ccmcp::apps::parse_options(argc, argv, options, 2);

  if (!config.redis_uri.has_value()) {
    std::cerr << "Error: --redis <uri> is required\n";
    return 1;
  }

  const auto parsed = ccmcp::interaction::parse_redis_uri(config.redis_uri.value());
  if (!parsed.has_value()) {
    std::cerr << "Error: invalid Redis URI '" << config.redis_uri.value() << "'\n"
              << "Accepted formats: tcp://host:port, redis://host:port, tcp://host\n";
    return 1;
  }

  const auto result = ccmcp::interaction::redis_ping(config.redis_uri.value());
  if (result.reachable) {
    std::cout << "OK: Redis reachable at "
              << ccmcp::interaction::redis_config_to_log_string(parsed.value()) << "\n";
    return 0;
  }

  std::cerr << "ERROR: " << result.error << "\n";
  return 1;
}
