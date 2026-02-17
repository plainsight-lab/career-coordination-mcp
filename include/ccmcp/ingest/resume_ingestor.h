#pragma once

#include "ccmcp/core/clock.h"
#include "ccmcp/core/id_generator.h"
#include "ccmcp/ingest/ingest_result.h"

#include <memory>
#include <string>
#include <vector>

namespace ccmcp::ingest {

/// Options for resume ingestion
struct IngestOptions {
  std::optional<std::string> source_path;   // Original file path (for metadata)
  std::optional<std::string> extracted_at;  // Override timestamp (for deterministic tests)
  bool enable_hygiene{true};                // Apply deterministic hygiene normalization
};

/// Interface for resume ingestion
class IResumeIngestor {
 public:
  virtual ~IResumeIngestor() = default;

  /// Ingest resume from file path
  [[nodiscard]] virtual IngestResult ingest_file(const std::string& file_path,
                                                 const IngestOptions& options,
                                                 core::IIdGenerator& id_gen,
                                                 core::IClock& clock) = 0;

  /// Ingest resume from raw bytes (with explicit format hint)
  [[nodiscard]] virtual IngestResult ingest_bytes(
      const std::vector<uint8_t>& data,
      const std::string& format,  // "md", "txt", "docx", "pdf"
      const IngestOptions& options, core::IIdGenerator& id_gen, core::IClock& clock) = 0;
};

/// Factory function to create default resume ingestor implementation
[[nodiscard]] std::unique_ptr<IResumeIngestor> create_resume_ingestor();

}  // namespace ccmcp::ingest
