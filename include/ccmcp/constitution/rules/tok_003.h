#pragma once

#include "ccmcp/constitution/constitutional_rule.h"

namespace ccmcp::constitution {

// TOK-003: Token spans must be within resume bounds (FAIL severity)
// Validates span references if present:
// - start_line and end_line are >= 1
// - start_line <= end_line
// - Lines are within canonical resume line count (if available in context)
// Rationale: Prevents invalid line references
class Tok003 final : public ConstitutionalRule {
 public:
  Tok003() = default;

  [[nodiscard]] std::string_view rule_id() const noexcept override { return "TOK-003"; }
  [[nodiscard]] std::string_view version() const noexcept override { return "0.3.0"; }
  [[nodiscard]] std::string_view description() const noexcept override {
    return "Token spans must be within resume bounds";
  }

  [[nodiscard]] std::vector<Finding> Validate(const ArtifactEnvelope& envelope,
                                              const ValidationContext& context) const override;
};

}  // namespace ccmcp::constitution
