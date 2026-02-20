#pragma once

#ifdef CCMCP_TRANSPORT_BOUNDARY_GUARD
#error "Concrete storage/redis header included in a guarded translation unit — use interfaces only."
#endif

#include "ccmcp/storage/audit_log.h"
#include "ccmcp/storage/sqlite/sqlite_db.h"

#include <map>
#include <memory>
#include <mutex>

namespace ccmcp::storage::sqlite {

// SqliteAuditLog implements IAuditLog with SQLite backend.
// Maintains append-only log with deterministic ordering via idx column.
// Each event carries a SHA-256 hash chain linking it to the previous event in its trace.
// Thread-safe append operations using mutex for idx counter and hash state.
class SqliteAuditLog final : public IAuditLog {
 public:
  explicit SqliteAuditLog(std::shared_ptr<SqliteDb> db);

  void append(const AuditEvent& event) override;
  [[nodiscard]] std::vector<AuditEvent> query(const std::string& trace_id) const override;

 private:
  std::shared_ptr<SqliteDb> db_;

  // Per-trace append state: next index and last event_hash.
  // Both are fetched atomically under mutex_ to ensure chain consistency.
  mutable std::mutex mutex_;
  mutable std::map<std::string, int> trace_indices_;

  // Chain state (idx + previous_hash) fetched under a single lock acquisition.
  struct AppendState {
    int idx{0};
    std::string previous_hash{};
  };

  AppendState get_append_state(const std::string& trace_id);
};

}  // namespace ccmcp::storage::sqlite
