#include "ccmcp/core/id_generator.h"

#include <chrono>

namespace ccmcp::core {

std::string SystemIdGenerator::next(std::string_view prefix) {
  // Production: timestamp (microseconds) + counter for global uniqueness
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const auto c = counter_.fetch_add(1, std::memory_order_relaxed);
  return std::string(prefix) + "-" + std::to_string(micros) + "-" + std::to_string(c);
}

std::string DeterministicIdGenerator::next(std::string_view prefix) {
  // Deterministic: counter only for reproducible output
  const auto c = counter_.fetch_add(1, std::memory_order_relaxed);
  return std::string(prefix) + "-" + std::to_string(c);
}

}  // namespace ccmcp::core
