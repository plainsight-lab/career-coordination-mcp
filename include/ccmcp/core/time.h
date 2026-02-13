#pragma once

#include <chrono>
#include <cstdint>

namespace ccmcp::core {

using Clock = std::chrono::system_clock;
using Timestamp = Clock::time_point;

inline Timestamp now_utc() { return Clock::now(); }

inline std::int64_t to_unix_millis(const Timestamp ts) {
  return std::chrono::duration_cast<std::chrono::milliseconds>(ts.time_since_epoch()).count();
}

}  // namespace ccmcp::core
