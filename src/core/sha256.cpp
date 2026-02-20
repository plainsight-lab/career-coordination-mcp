#include "ccmcp/core/sha256.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <vector>

namespace ccmcp::core {

namespace {

// FIPS 180-4 §4.2.2 — SHA-256 initial hash values.
// First 32 bits of fractional parts of square roots of first 8 primes.
constexpr std::array<uint32_t, 8> kH0 = {
    0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
    0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u,
};

// FIPS 180-4 §4.2.2 — SHA-256 round constants.
// First 32 bits of fractional parts of cube roots of first 64 primes.
constexpr std::array<uint32_t, 64> kK = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u,
    0xab1c5ed5u, 0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu,
    0x9bdc06a7u, 0xc19bf174u, 0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu,
    0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau, 0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
    0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu,
    0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u, 0xa2bfe8a1u, 0xa81a664bu,
    0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u, 0x19a4c116u,
    0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u,
    0xc67178f2u,
};

constexpr uint32_t rotr32(uint32_t x, unsigned n) noexcept {
  return (x >> n) | (x << (32u - n));
}

constexpr uint32_t ch(uint32_t x, uint32_t y, uint32_t z) noexcept {
  return (x & y) ^ (~x & z);
}

constexpr uint32_t maj(uint32_t x, uint32_t y, uint32_t z) noexcept {
  return (x & y) ^ (x & z) ^ (y & z);
}

// FIPS 180-4 §4.1.2 — SHA-256 functions.
constexpr uint32_t sigma0_big(uint32_t x) noexcept {
  return rotr32(x, 2u) ^ rotr32(x, 13u) ^ rotr32(x, 22u);
}

constexpr uint32_t sigma1_big(uint32_t x) noexcept {
  return rotr32(x, 6u) ^ rotr32(x, 11u) ^ rotr32(x, 25u);
}

constexpr uint32_t sigma0_small(uint32_t x) noexcept {
  return rotr32(x, 7u) ^ rotr32(x, 18u) ^ (x >> 3u);
}

constexpr uint32_t sigma1_small(uint32_t x) noexcept {
  return rotr32(x, 17u) ^ rotr32(x, 19u) ^ (x >> 10u);
}

// Process one 512-bit (64-byte) block. Mutates state in place.
void process_block(std::array<uint32_t, 8>& state, const uint8_t* block) noexcept {
  std::array<uint32_t, 64> w{};

  // FIPS 180-4 §6.2.2 step 1 — prepare message schedule.
  for (unsigned i = 0; i < 16u; ++i) {
    w[i] = (static_cast<uint32_t>(block[i * 4u + 0u]) << 24u) |
           (static_cast<uint32_t>(block[i * 4u + 1u]) << 16u) |
           (static_cast<uint32_t>(block[i * 4u + 2u]) << 8u) |
           (static_cast<uint32_t>(block[i * 4u + 3u]));
  }
  for (unsigned i = 16u; i < 64u; ++i) {
    w[i] = sigma1_small(w[i - 2u]) + w[i - 7u] + sigma0_small(w[i - 15u]) + w[i - 16u];
  }

  // FIPS 180-4 §6.2.2 step 2 — initialize working variables.
  uint32_t a = state[0];
  uint32_t b = state[1];
  uint32_t c = state[2];
  uint32_t d = state[3];
  uint32_t e = state[4];
  uint32_t f = state[5];
  uint32_t g = state[6];
  uint32_t h = state[7];

  // FIPS 180-4 §6.2.2 step 3 — 64 rounds.
  for (unsigned i = 0; i < 64u; ++i) {
    const uint32_t t1 = h + sigma1_big(e) + ch(e, f, g) + kK[i] + w[i];
    const uint32_t t2 = sigma0_big(a) + maj(a, b, c);
    h = g;
    g = f;
    f = e;
    e = d + t1;
    d = c;
    c = b;
    b = a;
    a = t1 + t2;
  }

  // FIPS 180-4 §6.2.2 step 4 — compute intermediate hash value.
  state[0] += a;
  state[1] += b;
  state[2] += c;
  state[3] += d;
  state[4] += e;
  state[5] += f;
  state[6] += g;
  state[7] += h;
}

}  // namespace

std::string sha256_hex(std::string_view input) {
  // FIPS 180-4 §5.1.1 — padding.
  // Padded message length is the smallest multiple of 512 bits (64 bytes)
  // that accommodates: original message + 1 byte (0x80) + 8 bytes (bit length).
  const uint64_t bit_len = static_cast<uint64_t>(input.size()) * 8u;
  const size_t padded_size = ((input.size() + 9u + 63u) / 64u) * 64u;

  std::vector<uint8_t> msg(padded_size, 0u);
  std::memcpy(msg.data(), input.data(), input.size());
  msg[input.size()] = 0x80u;  // append bit '1' followed by zeroes

  // Append original bit length as 64-bit big-endian at the end.
  for (unsigned i = 0; i < 8u; ++i) {
    msg[padded_size - 8u + i] = static_cast<uint8_t>(bit_len >> ((7u - i) * 8u));
  }

  // FIPS 180-4 §6.2.2 — process blocks.
  auto state = kH0;
  for (size_t offset = 0; offset < padded_size; offset += 64u) {
    process_block(state, msg.data() + offset);
  }

  // Produce lower-case hex digest.
  std::ostringstream oss;
  oss << std::hex << std::setfill('0');
  for (const uint32_t word : state) {
    oss << std::setw(8) << word;
  }
  return oss.str();
}

}  // namespace ccmcp::core
