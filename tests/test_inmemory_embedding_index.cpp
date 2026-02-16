#include "ccmcp/vector/inmemory_embedding_index.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace ccmcp::vector;

TEST_CASE("InMemoryEmbeddingIndex::upsert and get work correctly", "[vector][index]") {
  InMemoryEmbeddingIndex index;
  Vector vec = {1.0f, 2.0f, 3.0f};

  index.upsert("key1", vec, "metadata1");

  auto retrieved = index.get("key1");
  REQUIRE(retrieved.has_value());
  REQUIRE(retrieved->size() == 3);
  CHECK((*retrieved)[0] == 1.0f);
  CHECK((*retrieved)[1] == 2.0f);
  CHECK((*retrieved)[2] == 3.0f);
}

TEST_CASE("InMemoryEmbeddingIndex::upsert replaces existing vector", "[vector][index]") {
  InMemoryEmbeddingIndex index;
  Vector vec1 = {1.0f, 2.0f, 3.0f};
  Vector vec2 = {4.0f, 5.0f, 6.0f};

  index.upsert("key1", vec1, "metadata1");
  index.upsert("key1", vec2, "metadata2");

  auto retrieved = index.get("key1");
  REQUIRE(retrieved.has_value());
  CHECK((*retrieved)[0] == 4.0f);
}

TEST_CASE("InMemoryEmbeddingIndex::get returns nullopt for missing key", "[vector][index]") {
  InMemoryEmbeddingIndex index;

  auto result = index.get("nonexistent");
  CHECK_FALSE(result.has_value());
}

TEST_CASE("InMemoryEmbeddingIndex::query computes cosine similarity correctly", "[vector][index]") {
  InMemoryEmbeddingIndex index;

  // Identical vectors have similarity 1.0
  Vector vec1 = {1.0f, 0.0f, 0.0f};
  Vector vec2 = {0.0f, 1.0f, 0.0f};
  Vector vec3 = {1.0f, 0.0f, 0.0f};  // Same as vec1

  index.upsert("key1", vec1, "meta1");
  index.upsert("key2", vec2, "meta2");
  index.upsert("key3", vec3, "meta3");

  Vector query = {1.0f, 0.0f, 0.0f};
  auto results = index.query(query, 3);

  REQUIRE(results.size() == 3);

  // First two results should have similarity 1.0 (key1 and key3)
  CHECK_THAT(results[0].score, Catch::Matchers::WithinRel(1.0, 1e-6));
  CHECK_THAT(results[1].score, Catch::Matchers::WithinRel(1.0, 1e-6));

  // Third result should have similarity 0.0 (key2, orthogonal)
  CHECK_THAT(results[2].score, Catch::Matchers::WithinAbs(0.0, 1e-6));
}

TEST_CASE("InMemoryEmbeddingIndex::query performs deterministic tie-breaking", "[vector][index]") {
  InMemoryEmbeddingIndex index;

  // Three identical vectors
  Vector vec = {1.0f, 0.0f, 0.0f};
  index.upsert("key-c", vec, "meta");
  index.upsert("key-a", vec, "meta");
  index.upsert("key-b", vec, "meta");

  Vector query = {1.0f, 0.0f, 0.0f};
  auto results = index.query(query, 3);

  REQUIRE(results.size() == 3);

  // All should have similarity 1.0
  CHECK_THAT(results[0].score, Catch::Matchers::WithinRel(1.0, 1e-6));
  CHECK_THAT(results[1].score, Catch::Matchers::WithinRel(1.0, 1e-6));
  CHECK_THAT(results[2].score, Catch::Matchers::WithinRel(1.0, 1e-6));

  // Tie-breaking should be lexicographic by key
  CHECK(results[0].key == "key-a");
  CHECK(results[1].key == "key-b");
  CHECK(results[2].key == "key-c");
}

TEST_CASE("InMemoryEmbeddingIndex::query respects top_k limit", "[vector][index]") {
  InMemoryEmbeddingIndex index;

  Vector vec = {1.0f, 0.0f};
  index.upsert("key1", vec, "meta");
  index.upsert("key2", vec, "meta");
  index.upsert("key3", vec, "meta");
  index.upsert("key4", vec, "meta");
  index.upsert("key5", vec, "meta");

  Vector query = {1.0f, 0.0f};
  auto results = index.query(query, 3);

  CHECK(results.size() == 3);
}

TEST_CASE("InMemoryEmbeddingIndex::query handles empty index", "[vector][index]") {
  InMemoryEmbeddingIndex index;

  Vector query = {1.0f, 0.0f};
  auto results = index.query(query, 5);

  CHECK(results.empty());
}
