#include "ccmcp/core/clock.h"

#include <chrono>
#include <iomanip>
#include <sstream>

namespace ccmcp::core {

std::string SystemClock::now_iso8601() {
  const auto now = std::chrono::system_clock::now();
  const auto time_t_now = std::chrono::system_clock::to_time_t(now);

  std::ostringstream oss;
  oss << std::put_time(std::gmtime(&time_t_now), "%Y-%m-%dT%H:%M:%SZ");
  return oss.str();
}

std::string FixedClock::now_iso8601() {
  return fixed_time_;
}

}  // namespace ccmcp::core
