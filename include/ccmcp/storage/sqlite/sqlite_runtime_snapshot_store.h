#pragma once

#ifdef CCMCP_TRANSPORT_BOUNDARY_GUARD
#error "Concrete storage/redis header included in a guarded translation unit — use interfaces only."
#endif

#include "ccmcp/storage/runtime_snapshot_store.h"
#include "ccmcp/storage/sqlite/sqlite_db.h"

#include <memory>

namespace ccmcp::storage::sqlite {

// SqliteRuntimeSnapshotStore persists RuntimeConfigSnapshot artifacts to the
// runtime_snapshots table (schema v7).
//
// Snapshots are immutable once written: save() uses a plain INSERT.
// run_id PRIMARY KEY guarantees at-most-once storage per run.
class SqliteRuntimeSnapshotStore final : public IRuntimeSnapshotStore {
 public:
  explicit SqliteRuntimeSnapshotStore(std::shared_ptr<SqliteDb> db);

  void save(const std::string& run_id, const std::string& snapshot_json,
            const std::string& snapshot_hash, const std::string& created_at) override;

  [[nodiscard]] std::optional<std::string> get_snapshot_json(
      const std::string& run_id) const override;

 private:
  std::shared_ptr<SqliteDb> db_;
};

}  // namespace ccmcp::storage::sqlite
