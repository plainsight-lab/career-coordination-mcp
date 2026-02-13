#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace ccmcp::core {

// Deterministic placeholder hashing for scaffolding. Replace with SHA-256 later.
std::uint64_t stable_hash64(std::string_view input);
std::string stable_hash64_hex(std::string_view input);

}  // namespace ccmcp::core
