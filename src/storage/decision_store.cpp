#include "ccmcp/storage/decision_store.h"

#include <algorithm>
#include <optional>

namespace ccmcp::storage {

void InMemoryDecisionStore::upsert(const domain::DecisionRecord& record) {
  // Replace if decision_id already exists, otherwise append.
  for (auto& existing : records_) {
    if (existing.decision_id == record.decision_id) {
      existing = record;
      return;
    }
  }
  records_.push_back(record);
}

std::optional<domain::DecisionRecord> InMemoryDecisionStore::get(
    const std::string& decision_id) const {
  for (const auto& r : records_) {
    if (r.decision_id == decision_id) {
      return r;
    }
  }
  return std::nullopt;
}

std::vector<domain::DecisionRecord> InMemoryDecisionStore::list_by_trace(
    const std::string& trace_id) const {
  std::vector<domain::DecisionRecord> result;
  for (const auto& r : records_) {
    if (r.trace_id == trace_id) {
      result.push_back(r);
    }
  }
  // Stable ordering: sort by decision_id ascending.
  std::sort(result.begin(), result.end(),
            [](const domain::DecisionRecord& a, const domain::DecisionRecord& b) {
              return a.decision_id < b.decision_id;
            });
  return result;
}

}  // namespace ccmcp::storage
