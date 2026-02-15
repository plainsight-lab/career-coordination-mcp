#pragma once

#include "ccmcp/constitution/constitutional_rule.h"

namespace ccmcp::constitution {

// SCHEMA-001: Ensure MatchReport structural integrity (BLOCK severity)
// Validates:
// - overall_score exists and >= 0
// - requirement_matches count is correct
// - RequirementMatch fields are well-formed
// - matched flag consistency with contributing_atom_id
class Schema001 final : public ConstitutionalRule {
 public:
  Schema001() = default;

  [[nodiscard]] std::string_view rule_id() const noexcept override { return "SCHEMA-001"; }
  [[nodiscard]] std::string_view version() const noexcept override { return "0.1.0"; }
  [[nodiscard]] std::string_view description() const noexcept override {
    return "Ensure MatchReport structural integrity";
  }

  [[nodiscard]] std::vector<Finding> Validate(const ArtifactEnvelope& envelope,
                                              const ValidationContext& context) const override;
};

}  // namespace ccmcp::constitution
