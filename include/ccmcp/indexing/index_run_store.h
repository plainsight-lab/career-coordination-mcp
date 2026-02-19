#pragma once

#include "ccmcp/indexing/index_run.h"

#include <optional>
#include <string>
#include <vector>

namespace ccmcp::indexing {

// IIndexRunStore persists and retrieves IndexRun and IndexEntry records.
// This interface is the contract between the index build pipeline and storage.
//
// Drift detection: get_last_source_hash() returns the source_hash from the most
// recent completed run for a given (artifact_id, artifact_type, provider_id,
// model_id, prompt_version) combination, enabling the pipeline to determine
// whether an artifact's canonical text has changed since it was last indexed.
class IIndexRunStore {
 public:
  virtual ~IIndexRunStore() = default;

  // Insert or replace a run record.
  virtual void upsert_run(const IndexRun& run) = 0;

  // Insert or replace an entry record.
  virtual void upsert_entry(const IndexEntry& entry) = 0;

  // Retrieve a run by run_id. Returns nullopt if not found.
  [[nodiscard]] virtual std::optional<IndexRun> get_run(const std::string& run_id) const = 0;

  // List all runs, ordered by run_id ascending.
  [[nodiscard]] virtual std::vector<IndexRun> list_runs() const = 0;

  // List all entries for a run, ordered by (artifact_type, artifact_id) ascending.
  [[nodiscard]] virtual std::vector<IndexEntry> get_entries_for_run(
      const std::string& run_id) const = 0;

  // Returns the source_hash from the last completed run that matches all five
  // filter dimensions. Returns nullopt if no matching completed run exists.
  [[nodiscard]] virtual std::optional<std::string> get_last_source_hash(
      const std::string& artifact_id, const std::string& artifact_type,
      const std::string& provider_id, const std::string& model_id,
      const std::string& prompt_version) const = 0;

  // Atomically allocate the next run_id from the persistent monotonic counter.
  // Returns "run-N" where N is a 1-based integer that increments with each call.
  // The counter is backed by the id_counters table (schema v6) and survives
  // process restarts, guaranteeing unique, ordered run IDs across CLI invocations.
  // Throws std::runtime_error if the id_counters table is absent (schema v6 not applied).
  [[nodiscard]] virtual std::string next_index_run_id() = 0;

 protected:
  IIndexRunStore() = default;
  IIndexRunStore(const IIndexRunStore&) = default;
  IIndexRunStore& operator=(const IIndexRunStore&) = default;
  IIndexRunStore(IIndexRunStore&&) = default;
  IIndexRunStore& operator=(IIndexRunStore&&) = default;
};

}  // namespace ccmcp::indexing
