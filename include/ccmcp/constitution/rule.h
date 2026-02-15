#pragma once

#include "ccmcp/constitution/artifact_view.h"
#include "ccmcp/constitution/finding.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace ccmcp::constitution {

// ArtifactEnvelope carries metadata and a typed artifact view for validation.
// The typed artifact (envelope.artifact) is authoritative for rule evaluation.
// The content field is optional serialized representation for audit/logging only.
struct ArtifactEnvelope {
  std::string artifact_id;
  std::string content;  // Optional serialized representation (not used by rules)
  std::vector<std::string> source_refs;
  std::shared_ptr<const ArtifactView> artifact;  // Typed artifact for validation

  // Returns the artifact type from the typed view (or kUnknown if null)
  [[nodiscard]] ArtifactType artifact_type() const noexcept {
    return artifact ? artifact->type() : ArtifactType::kUnknown;
  }
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
