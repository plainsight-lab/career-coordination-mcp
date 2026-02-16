#include "ccmcp/core/hashing.h"
#include "ccmcp/core/normalization.h"
#include "ccmcp/embedding/embedding_provider.h"

#include <algorithm>
#include <cmath>
#include <map>

namespace ccmcp::embedding {

vector::Vector NullEmbeddingProvider::embed_text(std::string_view /* text */) const {
  return {};  // Empty vector disables embedding retrieval
}

DeterministicStubEmbeddingProvider::DeterministicStubEmbeddingProvider(size_t dim)
    : dimension_(dim) {}

vector::Vector DeterministicStubEmbeddingProvider::embed_text(std::string_view text) const {
  if (dimension_ == 0) {
    return {};
  }

  // Strategy: Generate deterministic vector from text statistics
  // 1. Tokenize text
  // 2. Compute token frequency histogram
  // 3. Hash tokens to vector indices
  // 4. Normalize to unit vector

  vector::Vector embedding(dimension_, 0.0f);

  // Tokenize
  std::string text_str(text);
  auto tokens = core::tokenize_ascii(text_str);

  if (tokens.empty()) {
    return embedding;  // Zero vector for empty text
  }

  // Count token frequencies
  std::map<std::string, int> token_counts;
  for (const auto& token : tokens) {
    token_counts[token]++;
  }

  // Hash each token to vector indices and accumulate counts
  for (const auto& [token, count] : token_counts) {
    // Use deterministic hash to get vector index
    uint64_t hash = core::stable_hash64(token);
    size_t idx = hash % dimension_;

    // Accumulate count at hashed position
    embedding[idx] += static_cast<float>(count);

    // Also spread to adjacent indices for smoothing (deterministic)
    size_t idx_prev = (idx + dimension_ - 1) % dimension_;
    size_t idx_next = (idx + 1) % dimension_;
    embedding[idx_prev] += static_cast<float>(count) * 0.3f;
    embedding[idx_next] += static_cast<float>(count) * 0.3f;
  }

  // Normalize to unit vector (L2 norm)
  float norm = 0.0f;
  for (float val : embedding) {
    norm += val * val;
  }

  if (norm > 0.0f) {
    norm = std::sqrt(norm);
    for (float& val : embedding) {
      val /= norm;
    }
  }

  return embedding;
}

}  // namespace ccmcp::embedding
