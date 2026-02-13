#pragma once

#include <string>
#include <vector>

namespace ccmcp::constitution {

enum class FindingSeverity {
  kPass,
  kWarn,
  kFail,
  kBlock,
};

struct Finding {
  std::string rule_id;
  FindingSeverity severity{FindingSeverity::kPass};
  std::string message;
  std::vector<std::string> evidence_refs;
};

}  // namespace ccmcp::constitution
