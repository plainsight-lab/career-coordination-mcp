#include "ccmcp/vector/null_embedding_index.h"

namespace ccmcp::vector {

void NullEmbeddingIndex::upsert(const VectorKey& /*key*/, const Vector& /*embedding*/,
                                const std::string& /*metadata*/) {
  // No-op
}

std::vector<VectorSearchResult> NullEmbeddingIndex::query(const Vector& /*query_vector*/,
                                                          size_t /*top_k*/) const {
  return {};
}

std::optional<Vector> NullEmbeddingIndex::get(const VectorKey& /*key*/) const {
  return std::nullopt;
}

}  // namespace ccmcp::vector
