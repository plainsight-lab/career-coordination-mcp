#include "ccmcp/indexing/index_run.h"

#include <stdexcept>

namespace ccmcp::indexing {

std::string index_run_status_to_string(IndexRunStatus s) {
  switch (s) {
    case IndexRunStatus::kPending:
      return "pending";
    case IndexRunStatus::kRunning:
      return "running";
    case IndexRunStatus::kCompleted:
      return "completed";
    case IndexRunStatus::kFailed:
      return "failed";
  }
  return "unknown";
}

IndexRunStatus index_run_status_from_string(const std::string& s) {
  if (s == "pending")
    return IndexRunStatus::kPending;
  if (s == "running")
    return IndexRunStatus::kRunning;
  if (s == "completed")
    return IndexRunStatus::kCompleted;
  if (s == "failed")
    return IndexRunStatus::kFailed;
  throw std::invalid_argument("Unknown IndexRunStatus: " + s);
}

}  // namespace ccmcp::indexing
