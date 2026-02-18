#include "ccmcp/vector/sqlite_embedding_index.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cstdlib>
#include <filesystem>
#include <string>

using namespace ccmcp::vector;

// ─────────────────────────────────────────────────────────────────────────────
// Unit tests — always run; use ":memory:" so no file I/O is required.
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("SqliteEmbeddingIndex: upsert and get with in-memory database", "[vector][sqlite]") {
  SqliteEmbeddingIndex index(":memory:");
  Vector vec = {1.0f, 2.0f, 3.0f};

  index.upsert("key1", vec, "metadata1");

  auto retrieved = index.get("key1");
  REQUIRE(retrieved.has_value());
  REQUIRE(retrieved->size() == 3);
  CHECK((*retrieved)[0] == 1.0f);
  CHECK((*retrieved)[1] == 2.0f);
  CHECK((*retrieved)[2] == 3.0f);
}

TEST_CASE("SqliteEmbeddingIndex: upsert replaces existing vector", "[vector][sqlite]") {
  SqliteEmbeddingIndex index(":memory:");
  Vector vec1 = {1.0f, 2.0f, 3.0f};
  Vector vec2 = {4.0f, 5.0f, 6.0f};

  index.upsert("key1", vec1, "metadata1");
  index.upsert("key1", vec2, "metadata2");

  auto retrieved = index.get("key1");
  REQUIRE(retrieved.has_value());
  CHECK((*retrieved)[0] == 4.0f);
  CHECK((*retrieved)[1] == 5.0f);
  CHECK((*retrieved)[2] == 6.0f);
}

TEST_CASE("SqliteEmbeddingIndex: get returns nullopt for missing key", "[vector][sqlite]") {
  SqliteEmbeddingIndex index(":memory:");
  auto result = index.get("nonexistent");
  CHECK_FALSE(result.has_value());
}

TEST_CASE("SqliteEmbeddingIndex: query computes cosine similarity correctly", "[vector][sqlite]") {
  SqliteEmbeddingIndex index(":memory:");

  // Identical unit vectors — similarity to matching query is 1.0; orthogonal is 0.0.
  Vector vec1 = {1.0f, 0.0f, 0.0f};
  Vector vec2 = {0.0f, 1.0f, 0.0f};
  Vector vec3 = {1.0f, 0.0f, 0.0f};  // Same direction as vec1

  index.upsert("key1", vec1, "meta1");
  index.upsert("key2", vec2, "meta2");
  index.upsert("key3", vec3, "meta3");

  Vector query = {1.0f, 0.0f, 0.0f};
  auto results = index.query(query, 3);

  REQUIRE(results.size() == 3);
  // key1 and key3 are identical to query (score 1.0); key2 is orthogonal (score 0.0).
  CHECK_THAT(results[0].score, Catch::Matchers::WithinRel(1.0, 1e-6));
  CHECK_THAT(results[1].score, Catch::Matchers::WithinRel(1.0, 1e-6));
  CHECK_THAT(results[2].score, Catch::Matchers::WithinAbs(0.0, 1e-6));
}

TEST_CASE("SqliteEmbeddingIndex: query performs deterministic tie-breaking by key",
          "[vector][sqlite]") {
  SqliteEmbeddingIndex index(":memory:");

  // Three identical vectors — all scores tie at 1.0; tie-break must be lexicographic.
  Vector vec = {1.0f, 0.0f, 0.0f};
  index.upsert("key-c", vec, "meta");
  index.upsert("key-a", vec, "meta");
  index.upsert("key-b", vec, "meta");

  Vector query = {1.0f, 0.0f, 0.0f};
  auto results = index.query(query, 3);

  REQUIRE(results.size() == 3);
  CHECK_THAT(results[0].score, Catch::Matchers::WithinRel(1.0, 1e-6));
  CHECK_THAT(results[1].score, Catch::Matchers::WithinRel(1.0, 1e-6));
  CHECK_THAT(results[2].score, Catch::Matchers::WithinRel(1.0, 1e-6));
  CHECK(results[0].key == "key-a");
  CHECK(results[1].key == "key-b");
  CHECK(results[2].key == "key-c");
}

TEST_CASE("SqliteEmbeddingIndex: query respects top_k limit", "[vector][sqlite]") {
  SqliteEmbeddingIndex index(":memory:");

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

TEST_CASE("SqliteEmbeddingIndex: query returns empty for empty index", "[vector][sqlite]") {
  SqliteEmbeddingIndex index(":memory:");
  Vector query = {1.0f, 0.0f};
  auto results = index.query(query, 5);
  CHECK(results.empty());
}

TEST_CASE("SqliteEmbeddingIndex: metadata is stored and retrieved via query", "[vector][sqlite]") {
  SqliteEmbeddingIndex index(":memory:");
  Vector vec = {1.0f, 0.0f};
  index.upsert("key1", vec, R"({"atom_id":"atom-001","domain":"cpp"})");

  Vector query = {1.0f, 0.0f};
  auto results = index.query(query, 1);

  REQUIRE(results.size() == 1);
  CHECK(results[0].metadata == R"({"atom_id":"atom-001","domain":"cpp"})");
}

TEST_CASE("SqliteEmbeddingIndex: float round-trip via BLOB is exact", "[vector][sqlite]") {
  SqliteEmbeddingIndex index(":memory:");

  // Values chosen to exercise float precision (memcpy round-trip must be exact).
  Vector original = {0.1f, 0.2f, 0.3f, -0.5f, 1.0f};
  index.upsert("precision-test", original, "{}");

  auto retrieved = index.get("precision-test");
  REQUIRE(retrieved.has_value());
  REQUIRE(retrieved->size() == original.size());
  for (size_t i = 0; i < original.size(); ++i) {
    CHECK((*retrieved)[i] == original[i]);  // Exact: memcpy round-trip guarantees bit-identity.
  }
}

TEST_CASE("SqliteEmbeddingIndex: zero-magnitude query vector returns zero scores",
          "[vector][sqlite]") {
  SqliteEmbeddingIndex index(":memory:");
  index.upsert("key1", {1.0f, 0.0f}, "meta");

  // Zero-magnitude query — cosine_similarity must return 0.0, not NaN.
  Vector zero_query = {0.0f, 0.0f};
  auto results = index.query(zero_query, 1);

  REQUIRE(results.size() == 1);
  CHECK_THAT(results[0].score, Catch::Matchers::WithinAbs(0.0, 1e-9));
}

// ─────────────────────────────────────────────────────────────────────────────
// Integration tests — opt-in via CCMCP_TEST_LANCEDB=1.
// These use real file paths to verify persistence, path wiring, and tie-breaking
// across open/close cycles.
// ─────────────────────────────────────────────────────────────────────────────

static bool should_run_lancedb_tests() {
  const char* env = std::getenv("CCMCP_TEST_LANCEDB");
  return env != nullptr && std::string(env) == "1";
}

TEST_CASE("SqliteEmbeddingIndex: file-backed persistence survives open/close",
          "[vector][sqlite][integration]") {
  if (!should_run_lancedb_tests()) {
    SKIP("SQLite vector integration tests disabled (set CCMCP_TEST_LANCEDB=1 to enable)");
  }

  const std::filesystem::path tmp_dir =
      std::filesystem::temp_directory_path() / "ccmcp_test_lancedb_persist";
  std::filesystem::create_directories(tmp_dir);
  const std::string db_path = (tmp_dir / "vectors.db").string();

  // Write scope: upsert three orthogonal vectors.
  {
    SqliteEmbeddingIndex index(db_path);
    index.upsert("atom-alpha", {1.0f, 0.0f, 0.0f}, R"({"atom":"alpha"})");
    index.upsert("atom-beta", {0.0f, 1.0f, 0.0f}, R"({"atom":"beta"})");
    index.upsert("atom-gamma", {0.0f, 0.0f, 1.0f}, R"({"atom":"gamma"})");
  }

  // Read scope: reopen and verify all data persisted.
  {
    SqliteEmbeddingIndex index(db_path);

    auto alpha = index.get("atom-alpha");
    REQUIRE(alpha.has_value());
    CHECK((*alpha)[0] == 1.0f);
    CHECK((*alpha)[1] == 0.0f);
    CHECK((*alpha)[2] == 0.0f);

    // Query {1,0,0} should rank atom-alpha first (similarity 1.0); others score 0.0.
    const Vector query = {1.0f, 0.0f, 0.0f};
    auto results = index.query(query, 3);

    REQUIRE(results.size() == 3);
    CHECK(results[0].key == "atom-alpha");
    CHECK_THAT(results[0].score, Catch::Matchers::WithinRel(1.0, 1e-6));
    CHECK_THAT(results[1].score, Catch::Matchers::WithinAbs(0.0, 1e-6));
    CHECK_THAT(results[2].score, Catch::Matchers::WithinAbs(0.0, 1e-6));
  }

  std::filesystem::remove_all(tmp_dir);
}

TEST_CASE("SqliteEmbeddingIndex: file-backed tie-breaking is deterministic across open/close",
          "[vector][sqlite][integration]") {
  if (!should_run_lancedb_tests()) {
    SKIP("SQLite vector integration tests disabled (set CCMCP_TEST_LANCEDB=1 to enable)");
  }

  const std::filesystem::path tmp_dir =
      std::filesystem::temp_directory_path() / "ccmcp_test_lancedb_tiebreak";
  std::filesystem::create_directories(tmp_dir);
  const std::string db_path = (tmp_dir / "vectors.db").string();

  // Insert three identical vectors in intentionally non-lexicographic order.
  {
    SqliteEmbeddingIndex index(db_path);
    const Vector vec = {1.0f, 0.0f, 0.0f};
    index.upsert("atom-z", vec, "{}");
    index.upsert("atom-a", vec, "{}");
    index.upsert("atom-m", vec, "{}");
  }

  // Reopen: tie-breaking must be by key, not by insertion order or SQLite row order.
  {
    SqliteEmbeddingIndex index(db_path);
    const Vector query = {1.0f, 0.0f, 0.0f};
    auto results = index.query(query, 3);

    REQUIRE(results.size() == 3);
    CHECK(results[0].key == "atom-a");
    CHECK(results[1].key == "atom-m");
    CHECK(results[2].key == "atom-z");
  }

  std::filesystem::remove_all(tmp_dir);
}

TEST_CASE("SqliteEmbeddingIndex: file-backed upsert replaces existing vector",
          "[vector][sqlite][integration]") {
  if (!should_run_lancedb_tests()) {
    SKIP("SQLite vector integration tests disabled (set CCMCP_TEST_LANCEDB=1 to enable)");
  }

  const std::filesystem::path tmp_dir =
      std::filesystem::temp_directory_path() / "ccmcp_test_lancedb_replace";
  std::filesystem::create_directories(tmp_dir);
  const std::string db_path = (tmp_dir / "vectors.db").string();

  {
    SqliteEmbeddingIndex index(db_path);
    index.upsert("atom-one", {1.0f, 0.0f}, R"({"version":1})");
  }
  {
    SqliteEmbeddingIndex index(db_path);
    index.upsert("atom-one", {0.0f, 1.0f}, R"({"version":2})");
  }
  {
    SqliteEmbeddingIndex index(db_path);
    auto result = index.get("atom-one");
    REQUIRE(result.has_value());
    CHECK((*result)[0] == 0.0f);
    CHECK((*result)[1] == 1.0f);

    // Verify metadata was updated too.
    const Vector query = {0.0f, 1.0f};
    auto qr = index.query(query, 1);
    REQUIRE(qr.size() == 1);
    CHECK(qr[0].metadata == R"({"version":2})");
  }

  std::filesystem::remove_all(tmp_dir);
}
