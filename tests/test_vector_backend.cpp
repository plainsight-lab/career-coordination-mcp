#include "ccmcp/vector/vector_backend.h"

#include <catch2/catch_test_macros.hpp>

using namespace ccmcp::vector;

TEST_CASE("parse_vector_backend: known flag values map to correct enumerators",
          "[vector][vector_backend]") {
  CHECK(parse_vector_backend("inmemory") == std::optional{VectorBackend::kInMemory});
  CHECK(parse_vector_backend("sqlite") == std::optional{VectorBackend::kSqlite});
  CHECK(parse_vector_backend("lancedb") == std::optional{VectorBackend::kLanceDb});
}

TEST_CASE("parse_vector_backend: unrecognised values return nullopt", "[vector][vector_backend]") {
  CHECK(!parse_vector_backend("").has_value());
  CHECK(!parse_vector_backend("sqlite3").has_value());
  CHECK(!parse_vector_backend("memory").has_value());
  CHECK(!parse_vector_backend("null").has_value());
  CHECK(!parse_vector_backend("InMemory").has_value());  // case-sensitive
  CHECK(!parse_vector_backend("SQLite").has_value());    // case-sensitive
  CHECK(!parse_vector_backend("LanceDB").has_value());   // case-sensitive
  CHECK(!parse_vector_backend("lancedb2").has_value());
}

TEST_CASE("to_string: known enumerators return canonical flag strings",
          "[vector][vector_backend]") {
  CHECK(to_string(VectorBackend::kInMemory) == "inmemory");
  CHECK(to_string(VectorBackend::kSqlite) == "sqlite");
  CHECK(to_string(VectorBackend::kLanceDb) == "lancedb");
}

TEST_CASE("to_string and parse_vector_backend roundtrip for all enumerators",
          "[vector][vector_backend]") {
  // Every enumerator must round-trip through to_string -> parse_vector_backend.
  for (auto b : {VectorBackend::kInMemory, VectorBackend::kSqlite, VectorBackend::kLanceDb}) {
    const std::string s{to_string(b)};
    const auto parsed = parse_vector_backend(s);
    REQUIRE(parsed.has_value());
    CHECK(parsed.value() == b);
  }
}
