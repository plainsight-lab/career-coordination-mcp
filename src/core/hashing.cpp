#include "ccmcp/core/hashing.h"

#include <iomanip>
#include <sstream>

namespace ccmcp::core {

std::uint64_t stable_hash64(const std::string_view input) {
  // FNV-1a 64-bit for deterministic placeholder hashing in v0.1 scaffolding.
  constexpr std::uint64_t kOffset = 14695981039346656037ull;
  constexpr std::uint64_t kPrime = 1099511628211ull;

  std::uint64_t hash = kOffset;
  for (const unsigned char c : input) {
    hash ^= static_cast<std::uint64_t>(c);
    hash *= kPrime;
  }
  return hash;
}

std::string stable_hash64_hex(const std::string_view input) {
  std::ostringstream oss;
  oss << std::hex << std::setfill('0') << std::setw(16) << stable_hash64(input);
  return oss.str();
}

}  // namespace ccmcp::core
