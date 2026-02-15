#pragma once

#include "ccmcp/constitution/finding.h"

#include <string_view>
#include <vector>

namespace ccmcp::constitution {

// Forward declarations
struct ArtifactEnvelope;
struct ValidationContext;

// ConstitutionalRule is the abstract base class for all validation rules.
// Rules validate typed artifacts via ArtifactView, not serialized JSON.
class ConstitutionalRule {
 public:
  virtual ~ConstitutionalRule() = default;

  // Rule metadata
  [[nodiscard]] virtual std::string_view rule_id() const noexcept = 0;
  [[nodiscard]] virtual std::string_view version() const noexcept = 0;
  [[nodiscard]] virtual std::string_view description() const noexcept = 0;

  // Validate the artifact and return findings.
  // Rules access envelope.artifact (typed view), not envelope.content (serialized string).
  [[nodiscard]] virtual std::vector<Finding> Validate(
      const ArtifactEnvelope& envelope, const ValidationContext& context) const = 0;

 protected:
  ConstitutionalRule() = default;
  ConstitutionalRule(const ConstitutionalRule&) = default;
  ConstitutionalRule& operator=(const ConstitutionalRule&) = default;
  ConstitutionalRule(ConstitutionalRule&&) = default;
  ConstitutionalRule& operator=(ConstitutionalRule&&) = default;
};

}  // namespace ccmcp::constitution
