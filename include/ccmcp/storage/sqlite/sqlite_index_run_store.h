#pragma once

#include "ccmcp/indexing/index_run_store.h"
#include "ccmcp/storage/sqlite/sqlite_db.h"

#include <memory>

// Forward declare sqlite3_stmt to avoid including sqlite3.h in this header.
struct sqlite3_stmt;

namespace ccmcp::storage::sqlite {

// SqliteIndexRunStore persists IndexRun and IndexEntry records to the
// index_runs and index_entries tables (schema v4).
//
// All queries use deterministic ORDER BY clauses to guarantee reproducible
// test output. NULL timestamp columns are mapped to std::nullopt.
class SqliteIndexRunStore final : public indexing::IIndexRunStore {
 public:
  explicit SqliteIndexRunStore(std::shared_ptr<SqliteDb> db);

  void upsert_run(const indexing::IndexRun& run) override;
  void upsert_entry(const indexing::IndexEntry& entry) override;

  [[nodiscard]] std::optional<indexing::IndexRun> get_run(const std::string& run_id) const override;
  [[nodiscard]] std::vector<indexing::IndexRun> list_runs() const override;
  [[nodiscard]] std::vector<indexing::IndexEntry> get_entries_for_run(
      const std::string& run_id) const override;
  [[nodiscard]] std::optional<std::string> get_last_source_hash(
      const std::string& artifact_id, const std::string& artifact_type,
      const std::string& provider_id, const std::string& model_id,
      const std::string& prompt_version) const override;

  [[nodiscard]] std::string next_index_run_id() override;

 private:
  std::shared_ptr<SqliteDb> db_;

  // Deserialize helpers — move data from a prepared statement row into a struct.
  [[nodiscard]] indexing::IndexRun row_to_run(sqlite3_stmt* stmt) const;
  [[nodiscard]] indexing::IndexEntry row_to_entry(sqlite3_stmt* stmt) const;
};

}  // namespace ccmcp::storage::sqlite
