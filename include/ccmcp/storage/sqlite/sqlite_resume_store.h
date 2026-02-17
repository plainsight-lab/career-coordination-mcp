#pragma once

#include "ccmcp/ingest/resume_store.h"
#include "ccmcp/storage/sqlite/sqlite_db.h"

#include <memory>

namespace ccmcp::storage::sqlite {

// SqliteResumeStore implements IResumeStore with SQLite backend.
// Uses prepared statements for all operations.
// Guarantees deterministic ordering (ORDER BY resume_id).
class SqliteResumeStore final : public ingest::IResumeStore {
 public:
  explicit SqliteResumeStore(std::shared_ptr<SqliteDb> db);

  void upsert(const ingest::IngestedResume& resume) override;
  [[nodiscard]] std::optional<ingest::IngestedResume> get(const core::ResumeId& id) const override;
  [[nodiscard]] std::optional<ingest::IngestedResume> get_by_hash(
      const std::string& resume_hash) const override;
  [[nodiscard]] std::vector<ingest::IngestedResume> list_all() const override;

 private:
  std::shared_ptr<SqliteDb> db_;

  // Helper to deserialize resume from prepared statement row
  [[nodiscard]] ingest::IngestedResume row_to_resume(sqlite3_stmt* stmt) const;

  // Helper to get resume_meta for a given resume_id
  [[nodiscard]] ingest::ResumeMeta get_meta(const std::string& resume_id) const;
};

}  // namespace ccmcp::storage::sqlite
