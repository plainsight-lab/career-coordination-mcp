#include "ccmcp/vector/lancedb_embedding_index.h"

#include <stdexcept>

namespace ccmcp::vector {

void LanceDBEmbeddingIndex::upsert(const VectorKey& /*key*/, const Vector& /*embedding*/,
                                   const std::string& /*metadata*/) {
  throw std::runtime_error("LanceDB not implemented in v0.2");
}

std::vector<VectorSearchResult> LanceDBEmbeddingIndex::query(const Vector& /*query_vector*/,
                                                             size_t /*top_k*/) const {
  throw std::runtime_error("LanceDB not implemented in v0.2");
}

std::optional<Vector> LanceDBEmbeddingIndex::get(const VectorKey& /*key*/) const {
  throw std::runtime_error("LanceDB not implemented in v0.2");
}

}  // namespace ccmcp::vector
