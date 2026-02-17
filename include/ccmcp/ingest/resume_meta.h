#pragma once

#include <optional>
#include <string>

namespace ccmcp::ingest {

/// Metadata about resume ingestion
struct ResumeMeta {
  std::optional<std::string> source_path;   // Original file path (if provided)
  std::string source_hash;                  // SHA256 of source bytes
  std::string extraction_method;            // e.g., "md-pass-through-v1"
  std::optional<std::string> extracted_at;  // ISO-8601 timestamp (injectable for tests)
  std::string ingestion_version{"0.3"};     // Ingestion pipeline version
};

}  // namespace ccmcp::ingest
