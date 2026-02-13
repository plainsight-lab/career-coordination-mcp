#pragma once

#include <functional>
#include <string>
#include <vector>

#include "ccmcp/constitution/finding.h"

namespace ccmcp::constitution {

struct ArtifactEnvelope {
  std::string artifact_id;
  std::string artifact_type;
  std::string content;
  std::vector<std::string> source_refs;
};

struct ValidationContext {
  std::string constitution_id;
  std::string constitution_version;
  std::vector<std::string> ground_truth_refs;
  std::string trace_id;
};

struct Rule {
  std::string rule_id;
  std::string version;
  std::string description;
  std::function<std::vector<Finding>(const ArtifactEnvelope&, const ValidationContext&)> evaluate;
};

}  // namespace ccmcp::constitution
