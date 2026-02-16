#include "ccmcp/vector/inmemory_embedding_index.h"

#include <algorithm>
#include <cmath>

namespace ccmcp::vector {

void InMemoryEmbeddingIndex::upsert(const VectorKey& key, const Vector& embedding,
                                    const std::string& metadata) {
  vectors_[key] = std::make_pair(embedding, metadata);
}

std::vector<VectorSearchResult> InMemoryEmbeddingIndex::query(const Vector& query_vector,
                                                              size_t top_k) const {
  std::vector<VectorSearchResult> results;
  results.reserve(vectors_.size());

  // Compute similarity for all vectors
  for (const auto& [key, pair] : vectors_) {
    const auto& [embedding, metadata] = pair;
    double score = cosine_similarity(query_vector, embedding);
    results.push_back(VectorSearchResult{.key = key, .score = score, .metadata = metadata});
  }

  // Sort by score descending, then by key ascending (deterministic tie-breaking)
  std::sort(results.begin(), results.end(),
            [](const VectorSearchResult& a, const VectorSearchResult& b) {
              constexpr double kEpsilon = 1e-9;
              if (std::abs(a.score - b.score) > kEpsilon) {
                return a.score > b.score;  // Higher score first
              }
              return a.key < b.key;  // Lexicographic tie-breaking
            });

  // Return top_k results
  if (results.size() > top_k) {
    results.resize(top_k);
  }

  return results;
}

std::optional<Vector> InMemoryEmbeddingIndex::get(const VectorKey& key) const {
  auto it = vectors_.find(key);
  if (it != vectors_.end()) {
    return it->second.first;
  }
  return std::nullopt;
}

double InMemoryEmbeddingIndex::cosine_similarity(const Vector& a, const Vector& b) {
  if (a.size() != b.size() || a.empty()) {
    return 0.0;
  }

  double dot_product = 0.0;
  double norm_a = 0.0;
  double norm_b = 0.0;

  for (size_t i = 0; i < a.size(); ++i) {
    dot_product += a[i] * b[i];
    norm_a += a[i] * a[i];
    norm_b += b[i] * b[i];
  }

  norm_a = std::sqrt(norm_a);
  norm_b = std::sqrt(norm_b);

  if (norm_a == 0.0 || norm_b == 0.0) {
    return 0.0;
  }

  return dot_product / (norm_a * norm_b);
}

}  // namespace ccmcp::vector
