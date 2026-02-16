#pragma once

#include "ccmcp/vector/embedding_index.h"

#include <map>
#include <utility>

namespace ccmcp::vector {

// InMemoryEmbeddingIndex stores vectors in-memory using std::map.
// Uses cosine similarity for query operations with deterministic tie-breaking.
// Suitable for testing and small-scale v0.2 development.
class InMemoryEmbeddingIndex final : public IEmbeddingIndex {
 public:
  void upsert(const VectorKey& key, const Vector& embedding, const std::string& metadata) override;

  [[nodiscard]] std::vector<VectorSearchResult> query(const Vector& query_vector,
                                                      size_t top_k) const override;

  [[nodiscard]] std::optional<Vector> get(const VectorKey& key) const override;

 private:
  std::map<VectorKey, std::pair<Vector, std::string>> vectors_;

  // Compute cosine similarity between two vectors.
  // Returns 0.0 if either vector has zero magnitude.
  [[nodiscard]] static double cosine_similarity(const Vector& a, const Vector& b);
};

}  // namespace ccmcp::vector
