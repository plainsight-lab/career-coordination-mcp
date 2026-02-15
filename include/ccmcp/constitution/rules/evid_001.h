#pragma once

#include "ccmcp/constitution/constitutional_rule.h"

namespace ccmcp::constitution {

// EVID-001: Ensure evidence attribution for matched requirements (FAIL severity)
// Validates:
// - Matched requirements have non-empty contributing_atom_id
// - Matched requirements have non-empty evidence_tokens
class Evid001 final : public ConstitutionalRule {
 public:
  Evid001() = default;

  [[nodiscard]] std::string_view rule_id() const noexcept override { return "EVID-001"; }
  [[nodiscard]] std::string_view version() const noexcept override { return "0.1.0"; }
  [[nodiscard]] std::string_view description() const noexcept override {
    return "Ensure evidence attribution for matched requirements";
  }

  [[nodiscard]] std::vector<Finding> Validate(const ArtifactEnvelope& envelope,
                                               const ValidationContext& context) const override;
};

}  // namespace ccmcp::constitution
