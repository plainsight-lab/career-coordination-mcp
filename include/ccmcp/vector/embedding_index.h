#pragma once

#include <optional>
#include <string>
#include <vector>

namespace ccmcp::vector {

using Vector = std::vector<float>;
using VectorKey = std::string;  // AtomId.value or similar

struct VectorSearchResult {
  VectorKey key;         // NOLINT(readability-identifier-naming)
  double score;          // NOLINT(readability-identifier-naming)
  std::string metadata;  // NOLINT(readability-identifier-naming)
};

// IEmbeddingIndex defines the interface for vector similarity search.
// Implementations may use in-memory storage (for testing), LanceDB (for production),
// or other vector databases.
class IEmbeddingIndex {
 public:
  virtual ~IEmbeddingIndex() = default;

  // upsert inserts or updates a vector with associated metadata.
  virtual void upsert(const VectorKey& key, const Vector& embedding,
                      const std::string& metadata) = 0;

  // query performs similarity search and returns top_k results.
  // Results are sorted by score (descending), with deterministic tie-breaking.
  [[nodiscard]] virtual std::vector<VectorSearchResult> query(const Vector& query_vector,
                                                              size_t top_k) const = 0;

  // get retrieves the stored embedding for a given key.
  [[nodiscard]] virtual std::optional<Vector> get(const VectorKey& key) const = 0;
};

}  // namespace ccmcp::vector
