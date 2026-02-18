#pragma once

#include "ccmcp/domain/decision_record.h"

#include <optional>
#include <string>
#include <vector>

namespace ccmcp::storage {

// IDecisionStore persists and retrieves DecisionRecord artifacts.
// Decision records are append-only: upsert may overwrite by decision_id but
// the semantic intent is always "record the decision made at this point in time".
//
// list_by_trace() returns records ordered by decision_id ascending,
// giving a deterministic stable ordering that does not depend on insertion time.
class IDecisionStore {
 public:
  virtual ~IDecisionStore() = default;

  // Insert or replace a decision record by decision_id.
  virtual void upsert(const domain::DecisionRecord& record) = 0;

  // Retrieve a decision record by decision_id. Returns nullopt if not found.
  [[nodiscard]] virtual std::optional<domain::DecisionRecord> get(
      const std::string& decision_id) const = 0;

  // List all decision records for a given trace_id, ordered by decision_id ascending.
  [[nodiscard]] virtual std::vector<domain::DecisionRecord> list_by_trace(
      const std::string& trace_id) const = 0;

 protected:
  IDecisionStore() = default;
  IDecisionStore(const IDecisionStore&) = default;
  IDecisionStore& operator=(const IDecisionStore&) = default;
  IDecisionStore(IDecisionStore&&) = default;
  IDecisionStore& operator=(IDecisionStore&&) = default;
};

// In-memory implementation of IDecisionStore. Ephemeral — lost on process exit.
// Intended for unit tests only. Production paths (including ephemeral server mode)
// use SqliteDecisionStore backed by an in-memory SQLite DB to preserve the interface contract.
class InMemoryDecisionStore final : public IDecisionStore {
 public:
  void upsert(const domain::DecisionRecord& record) override;

  [[nodiscard]] std::optional<domain::DecisionRecord> get(
      const std::string& decision_id) const override;

  [[nodiscard]] std::vector<domain::DecisionRecord> list_by_trace(
      const std::string& trace_id) const override;

 private:
  // Ordered by insertion/update time. list_by_trace() sorts by decision_id before returning.
  std::vector<domain::DecisionRecord> records_;
};

}  // namespace ccmcp::storage
