#pragma once

#include "ccmcp/vector/embedding_index.h"

#include <memory>
#include <string>

// Forward-declare sqlite3 to avoid exposing the SQLite header in the public API.
struct sqlite3;

namespace ccmcp::vector {

// SqliteEmbeddingIndex is a persistent vector index backed by a dedicated SQLite database.
// It is selected via --vector-backend sqlite (with --vector-db-path specifying the directory).
//
// SQLite was chosen because it is already a project dependency and satisfies the same
// persistence, determinism, and interface-contract requirements as a purpose-built vector
// store. When a C++ LanceDB SDK becomes available in vcpkg, LanceDBEmbeddingIndex can be
// wired as --vector-backend lancedb; SqliteEmbeddingIndex remains available as a fallback.
//
// Storage: a standalone SQLite database at a caller-supplied file path.
//   Schema: embedding_vectors(key TEXT PK, vector_blob BLOB, dimension INT,
//                             metadata_json TEXT, created_at TEXT)
//
// Vectors are serialised as raw float32 bytes (native byte order) in the BLOB column.
// The database is a derived, rebuildable store — canonical truth stays in atoms/SQLite.
//
// Query: full-scan cosine similarity, identical algorithm to InMemoryEmbeddingIndex.
//   Tie-breaking: |score_a - score_b| <= 1e-9 → lexicographic key order (ascending).
//
// Thread safety: single-threaded (one connection per instance), same model as all
// other Sqlite* classes in this project.
class SqliteEmbeddingIndex final : public IEmbeddingIndex {
 public:
  // Opens or creates the SQLite database at db_path and ensures the schema is applied.
  // Throws std::runtime_error if the database cannot be opened or schema setup fails.
  explicit SqliteEmbeddingIndex(const std::string& db_path);

  // Defined in the .cpp so that the sqlite3 destructor is invoked where the type is complete.
  ~SqliteEmbeddingIndex();

  SqliteEmbeddingIndex(const SqliteEmbeddingIndex&) = delete;
  SqliteEmbeddingIndex& operator=(const SqliteEmbeddingIndex&) = delete;
  SqliteEmbeddingIndex(SqliteEmbeddingIndex&&) = delete;
  SqliteEmbeddingIndex& operator=(SqliteEmbeddingIndex&&) = delete;

  // Inserts or replaces the vector for key. Silent on failure (matches in-memory semantics).
  void upsert(const VectorKey& key, const Vector& embedding, const std::string& metadata) override;

  // Returns top_k results sorted by cosine similarity (desc), tie-broken by key (asc).
  [[nodiscard]] std::vector<VectorSearchResult> query(const Vector& query_vector,
                                                      size_t top_k) const override;

  // Returns the stored embedding for key, or nullopt if not found.
  [[nodiscard]] std::optional<Vector> get(const VectorKey& key) const override;

 private:
  struct DbDeleter {
    void operator()(sqlite3* db) const;
  };

  std::unique_ptr<sqlite3, DbDeleter> db_;

  // Creates the embedding_vectors table if absent.
  void ensure_schema();

  // Cosine similarity; returns 0.0 on dimension mismatch or zero-magnitude vector.
  [[nodiscard]] static double cosine_similarity(const Vector& a, const Vector& b);

  // Serialise a Vector to raw float32 bytes (native byte order).
  [[nodiscard]] static std::vector<std::byte> to_blob(const Vector& v);

  // Deserialise raw float32 bytes to a Vector.
  [[nodiscard]] static Vector from_blob(const void* data, int size_bytes);
};

}  // namespace ccmcp::vector
