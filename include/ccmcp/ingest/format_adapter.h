#pragma once

#include "ccmcp/core/result.h"

#include <string>
#include <vector>

namespace ccmcp::ingest {

/// Error type for extraction failures
struct ExtractionError {
  std::string message;
};

/// Result of format extraction
using ExtractionResult = core::Result<std::string, ExtractionError>;

/// Format adapter interface
class IFormatAdapter {
 public:
  virtual ~IFormatAdapter() = default;

  /// Extract markdown from raw bytes
  [[nodiscard]] virtual ExtractionResult extract(const std::vector<uint8_t>& data) const = 0;

  /// Get extraction method identifier (e.g., "md-pass-through-v1")
  [[nodiscard]] virtual std::string extraction_method() const = 0;
};

/// Markdown pass-through adapter (no conversion needed)
class MarkdownAdapter : public IFormatAdapter {
 public:
  [[nodiscard]] ExtractionResult extract(const std::vector<uint8_t>& data) const override;
  [[nodiscard]] std::string extraction_method() const override { return "md-pass-through-v1"; }
};

/// Plain text wrapper adapter (wraps text in markdown code fence)
class TextAdapter : public IFormatAdapter {
 public:
  [[nodiscard]] ExtractionResult extract(const std::vector<uint8_t>& data) const override;
  [[nodiscard]] std::string extraction_method() const override { return "txt-wrap-v1"; }
};

/// PDF text extraction adapter (placeholder - requires PDF library)
class PdfAdapter : public IFormatAdapter {
 public:
  [[nodiscard]] ExtractionResult extract(const std::vector<uint8_t>& data) const override;
  [[nodiscard]] std::string extraction_method() const override { return "pdf-text-extract-v1"; }
};

/// DOCX text extraction adapter (placeholder - requires DOCX library)
class DocxAdapter : public IFormatAdapter {
 public:
  [[nodiscard]] ExtractionResult extract(const std::vector<uint8_t>& data) const override;
  [[nodiscard]] std::string extraction_method() const override { return "docx-extract-v1"; }
};

}  // namespace ccmcp::ingest
