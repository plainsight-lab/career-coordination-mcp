#pragma once

#include "ccmcp/constitution/constitutional_rule.h"

namespace ccmcp::constitution {

// TOK-004: No hallucinated tokens (FAIL severity)
// Validates tokens are grounded in resume:
// - Each token must appear in canonical resume markdown OR
// - Be derivable via deterministic tokenization (tokenize_ascii)
// - Context must provide canonical resume text for verification
// Rationale: Prevents LLM hallucinations from corrupting token set
class Tok004 final : public ConstitutionalRule {
 public:
  Tok004() = default;

  [[nodiscard]] std::string_view rule_id() const noexcept override { return "TOK-004"; }
  [[nodiscard]] std::string_view version() const noexcept override { return "0.3.0"; }
  [[nodiscard]] std::string_view description() const noexcept override {
    return "No hallucinated tokens - must be derivable from resume";
  }

  [[nodiscard]] std::vector<Finding> Validate(const ArtifactEnvelope& envelope,
                                              const ValidationContext& context) const override;
};

}  // namespace ccmcp::constitution
