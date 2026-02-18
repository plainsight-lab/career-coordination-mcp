#pragma once

#include <optional>
#include <string>

namespace ccmcp::indexing {

// Status of an index build run.
// kPending: created but not yet started
// kRunning: in progress
// kCompleted: finished successfully
// kFailed: terminated with error
enum class IndexRunStatus { kPending, kRunning, kCompleted, kFailed };

// IndexRun records metadata about a single embedding index build run.
// started_at and completed_at are optional to support deterministic tests
// where timestamps are injected or absent.
struct IndexRun {
  std::string run_id;
  std::optional<std::string> started_at;
  std::optional<std::string> completed_at;
  std::string provider_id;
  std::string model_id;
  std::string prompt_version;
  IndexRunStatus status;
  std::string summary_json;
};

// IndexEntry records the provenance of a single artifact's embedding within a run.
// source_hash: stable_hash64_hex of the canonical text used to generate the embedding.
// vector_hash: stable_hash64_hex of the float bytes of the resulting embedding vector.
// indexed_at is optional to support deterministic tests.
struct IndexEntry {
  std::string run_id;
  std::string artifact_type;  // "atom" | "resume" | "opportunity"
  std::string artifact_id;
  std::string source_hash;
  std::string vector_hash;
  std::optional<std::string> indexed_at;
};

// Status string conversion helpers.
// index_run_status_from_string throws std::invalid_argument for unknown values.
std::string index_run_status_to_string(IndexRunStatus s);
IndexRunStatus index_run_status_from_string(const std::string& s);

}  // namespace ccmcp::indexing
