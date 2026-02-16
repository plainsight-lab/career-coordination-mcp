#pragma once

#include "ccmcp/storage/audit_log.h"
#include "ccmcp/storage/sqlite/sqlite_db.h"

#include <map>
#include <memory>
#include <mutex>

namespace ccmcp::storage::sqlite {

// SqliteAuditLog implements IAuditLog with SQLite backend.
// Maintains append-only log with deterministic ordering via idx column.
// Thread-safe append operations using mutex for idx counter.
class SqliteAuditLog final : public IAuditLog {
 public:
  explicit SqliteAuditLog(std::shared_ptr<SqliteDb> db);

  void append(const AuditEvent& event) override;
  [[nodiscard]] std::vector<AuditEvent> query(const std::string& trace_id) const override;

 private:
  std::shared_ptr<SqliteDb> db_;

  // Per-trace index counters (for deterministic ordering)
  mutable std::mutex mutex_;
  mutable std::map<std::string, int> trace_indices_;

  // Get next index for a trace (thread-safe)
  int get_next_index(const std::string& trace_id);
};

}  // namespace ccmcp::storage::sqlite
