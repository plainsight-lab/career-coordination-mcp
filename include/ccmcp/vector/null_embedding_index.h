#pragma once

#include "ccmcp/vector/embedding_index.h"

namespace ccmcp::vector {

// NullEmbeddingIndex is a no-op implementation of IEmbeddingIndex.
// All methods are no-ops or return empty results.
// Used when vector search is not needed or not yet implemented.
class NullEmbeddingIndex final : public IEmbeddingIndex {
 public:
  void upsert(const VectorKey& key, const Vector& embedding, const std::string& metadata) override;

  [[nodiscard]] std::vector<VectorSearchResult> query(const Vector& query_vector,
                                                      size_t top_k) const override;

  [[nodiscard]] std::optional<Vector> get(const VectorKey& key) const override;
};

}  // namespace ccmcp::vector
