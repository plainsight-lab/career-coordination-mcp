#pragma once

#include <string>
#include <string_view>

namespace ccmcp::core {

// sha256_hex returns the SHA-256 digest of input as a lower-case hex string.
//
// Implements FIPS 180-4 SHA-256.
// No external dependencies — pure C++20.
// Output: 64-character lower-case hexadecimal string.
[[nodiscard]] std::string sha256_hex(std::string_view input);

}  // namespace ccmcp::core
