#include "ccmcp/vector/null_embedding_index.h"

#include <catch2/catch_test_macros.hpp>

using namespace ccmcp::vector;

TEST_CASE("NullEmbeddingIndex::upsert is no-op", "[vector][index]") {
  NullEmbeddingIndex index;
  Vector vec = {1.0f, 2.0f, 3.0f};

  // Should not throw
  REQUIRE_NOTHROW(index.upsert("key1", vec, "metadata"));
}

TEST_CASE("NullEmbeddingIndex::query returns empty", "[vector][index]") {
  NullEmbeddingIndex index;
  Vector query_vec = {1.0f, 2.0f, 3.0f};

  auto results = index.query(query_vec, 5);
  CHECK(results.empty());
}

TEST_CASE("NullEmbeddingIndex::get returns nullopt", "[vector][index]") {
  NullEmbeddingIndex index;
  Vector vec = {1.0f, 2.0f, 3.0f};
  index.upsert("key1", vec, "metadata");

  auto result = index.get("key1");
  CHECK_FALSE(result.has_value());
}
