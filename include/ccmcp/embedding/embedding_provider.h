#pragma once

#include "ccmcp/vector/embedding_index.h"

#include <string_view>

namespace ccmcp::embedding {

// IEmbeddingProvider generates vector embeddings from text.
// This is a boundary for future integration with real embedding models.
// v0.2: Only deterministic stub implementations for testing.
class IEmbeddingProvider {
 public:
  virtual ~IEmbeddingProvider() = default;

  // embed_text converts text to a fixed-dimension vector.
  // Determinism: for the same text, must return the same vector.
  [[nodiscard]] virtual vector::Vector embed_text(std::string_view text) const = 0;

  // dimension returns the embedding vector dimension.
  [[nodiscard]] virtual size_t dimension() const = 0;
};

// NullEmbeddingProvider returns empty vectors (disables embedding retrieval).
class NullEmbeddingProvider final : public IEmbeddingProvider {
 public:
  [[nodiscard]] vector::Vector embed_text(std::string_view text) const override;
  [[nodiscard]] size_t dimension() const override { return 0; }
};

// DeterministicStubEmbeddingProvider generates stable vectors for testing.
// Strategy: Hash-based vector generation from token counts and character frequencies.
// Guarantees: Same text always produces same vector (deterministic).
class DeterministicStubEmbeddingProvider final : public IEmbeddingProvider {
 public:
  explicit DeterministicStubEmbeddingProvider(size_t dim = 128);

  [[nodiscard]] vector::Vector embed_text(std::string_view text) const override;
  [[nodiscard]] size_t dimension() const override { return dimension_; }

 private:
  size_t dimension_;
};

}  // namespace ccmcp::embedding
