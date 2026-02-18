#pragma once

#include "ccmcp/vector/embedding_index.h"

namespace ccmcp::vector {

// LanceDBEmbeddingIndex is a reserved stub for a future native LanceDB C++ SDK integration.
//
// Current status (v0.3): LanceDB has no official C++ client library available in vcpkg.
// The --vector-backend lancedb CLI flag is implemented via SqliteEmbeddingIndex, which
// provides the same IEmbeddingIndex contract backed by SQLite persistence.
//
// This stub remains as a marker and compile-time boundary for when a real C++ LanceDB SDK
// becomes available. Do not use this class directly — use SqliteEmbeddingIndex instead.
// All methods throw std::runtime_error to make accidental use immediately visible.
class LanceDBEmbeddingIndex final : public IEmbeddingIndex {
 public:
  void upsert(const VectorKey& key, const Vector& embedding, const std::string& metadata) override;

  [[nodiscard]] std::vector<VectorSearchResult> query(const Vector& query_vector,
                                                      size_t top_k) const override;

  [[nodiscard]] std::optional<Vector> get(const VectorKey& key) const override;
};

}  // namespace ccmcp::vector
