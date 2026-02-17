#pragma once

#include "ccmcp/core/ids.h"
#include "ccmcp/core/result.h"
#include "ccmcp/ingest/ingested_resume.h"

#include <optional>
#include <string>
#include <vector>

namespace ccmcp::ingest {

/// Interface for resume persistence
class IResumeStore {
 public:
  virtual ~IResumeStore() = default;

  /// Store an ingested resume
  virtual void upsert(const IngestedResume& resume) = 0;

  /// Get resume by ID
  [[nodiscard]] virtual std::optional<IngestedResume> get(const core::ResumeId& id) const = 0;

  /// Get resume by content hash (deterministic lookup)
  [[nodiscard]] virtual std::optional<IngestedResume> get_by_hash(
      const std::string& resume_hash) const = 0;

  /// List all resumes (sorted by resume_id for determinism)
  [[nodiscard]] virtual std::vector<IngestedResume> list_all() const = 0;
};

}  // namespace ccmcp::ingest
