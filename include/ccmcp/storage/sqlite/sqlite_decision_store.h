#pragma once

#ifdef CCMCP_TRANSPORT_BOUNDARY_GUARD
#error "Concrete storage/redis header included in a guarded translation unit — use interfaces only."
#endif

#include "ccmcp/storage/decision_store.h"
#include "ccmcp/storage/sqlite/sqlite_db.h"

#include <memory>

// Forward declare sqlite3_stmt to avoid including sqlite3.h in this header.
struct sqlite3_stmt;

namespace ccmcp::storage::sqlite {

// SqliteDecisionStore persists DecisionRecord artifacts to the decision_records table
// (schema v5). The full record is serialized as JSON in the decision_json column.
//
// Deterministic ordering: list_by_trace() orders by decision_id ASC.
// NULL created_at columns map to std::nullopt.
class SqliteDecisionStore final : public IDecisionStore {
 public:
  explicit SqliteDecisionStore(std::shared_ptr<SqliteDb> db);

  void upsert(const domain::DecisionRecord& record) override;

  [[nodiscard]] std::optional<domain::DecisionRecord> get(
      const std::string& decision_id) const override;

  [[nodiscard]] std::vector<domain::DecisionRecord> list_by_trace(
      const std::string& trace_id) const override;

 private:
  std::shared_ptr<SqliteDb> db_;

  // Deserialize a row from a prepared statement into a DecisionRecord.
  // Column order: decision_id(0), trace_id(1), opportunity_id(2),
  //               artifact_id(3), decision_json(4), created_at(5)
  [[nodiscard]] domain::DecisionRecord row_to_record(sqlite3_stmt* stmt) const;
};

}  // namespace ccmcp::storage::sqlite
