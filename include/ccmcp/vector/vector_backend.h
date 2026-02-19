#pragma once

// VectorBackend — authoritative vocabulary for the --vector-backend flag.
//
// C++ Core Guidelines Enum.1: prefer enumerations over macros.
// C++ Core Guidelines Enum.2: use enumerations to represent sets of related named constants.
//
// The compiler will warn on incomplete switch statements, providing exhaustiveness checking
// that raw string comparison cannot provide. Every caller that adds a new backend must
// update all switch sites or receive a compile-time diagnostic.
//
// CLI flag: --vector-backend <value>
// Valid runtime values: "inmemory", "sqlite"
// Reserved (fail-fast): "lancedb"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace ccmcp::vector {

// VectorBackend enumerates every recognised --vector-backend flag value.
//
// kLanceDb is RESERVED. LanceDB has no official C++ SDK in vcpkg as of v0.4.
// Requesting --vector-backend lancedb fails fast at startup with an actionable message.
// Use kSqlite for persistent vector storage.
//
// uint8_t base type: three enumerators fit in one byte; no reason to pay for int.
enum class VectorBackend : uint8_t {
  kInMemory,  // "inmemory" — InMemoryEmbeddingIndex (ephemeral, default)
  kSqlite,    // "sqlite"   — SqliteEmbeddingIndex   (persistent, requires --vector-db-path)
  kLanceDb,   // "lancedb"  — RESERVED: not yet implemented; process exits on startup
};

// parse_vector_backend parses a --vector-backend flag value into a VectorBackend.
// Returns std::nullopt for unrecognised values (including empty string).
// Case-sensitive: "sqlite" matches, "SQLite" does not.
[[nodiscard]] inline std::optional<VectorBackend> parse_vector_backend(const std::string& s) {
  if (s == "inmemory") {
    return VectorBackend::kInMemory;
  }
  if (s == "sqlite") {
    return VectorBackend::kSqlite;
  }
  if (s == "lancedb") {
    return VectorBackend::kLanceDb;
  }
  return std::nullopt;
}

// to_string returns the canonical flag string for a VectorBackend enumerator.
// The returned string_view is a string literal and is always valid.
[[nodiscard]] inline std::string_view to_string(VectorBackend b) {
  switch (b) {
    case VectorBackend::kInMemory:
      return "inmemory";
    case VectorBackend::kSqlite:
      return "sqlite";
    case VectorBackend::kLanceDb:
      return "lancedb";
  }
  return "unknown";  // unreachable — all enumerators covered above
}

}  // namespace ccmcp::vector
