#include "ccmcp/interaction/redis_health.h"

#include <sw/redis++/redis++.h>

namespace ccmcp::interaction {

RedisHealthResult redis_ping(const std::string& uri) {
  try {
    sw::redis::Redis redis(uri);
    redis.ping();
    return RedisHealthResult{true, ""};
  } catch (const std::exception& e) {
    return RedisHealthResult{false, e.what()};
  } catch (...) {
    return RedisHealthResult{false, "unknown error"};
  }
}

}  // namespace ccmcp::interaction
