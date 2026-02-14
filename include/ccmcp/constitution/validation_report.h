#pragma once

#include "ccmcp/constitution/finding.h"

#include <string>
#include <vector>

namespace ccmcp::constitution {

enum class ValidationStatus {
  kAccepted,
  kNeedsReview,
  kRejected,
  kBlocked,
};

struct ValidationReport {
  std::string report_id;
  std::string trace_id;
  std::string artifact_id;
  std::string constitution_id;
  std::string constitution_version;
  ValidationStatus status{ValidationStatus::kAccepted};
  std::vector<Finding> findings;
};

}  // namespace ccmcp::constitution
