#pragma once

#include "ccmcp/constitution/constitutional_rule.h"

namespace ccmcp::constitution {

// TOK-001: Token IR source_hash must match canonical resume hash (BLOCK severity)
// Validates provenance binding:
// - source_hash field exists
// - source_hash matches canonical resume hash from context
// Rationale: Prevents derived artifacts from being applied to wrong resume
class Tok001 final : public ConstitutionalRule {
 public:
  Tok001() = default;

  [[nodiscard]] std::string_view rule_id() const noexcept override { return "TOK-001"; }
  [[nodiscard]] std::string_view version() const noexcept override { return "0.3.0"; }
  [[nodiscard]] std::string_view description() const noexcept override {
    return "Token IR source_hash must match canonical resume hash";
  }

  [[nodiscard]] std::vector<Finding> Validate(const ArtifactEnvelope& envelope,
                                              const ValidationContext& context) const override;
};

}  // namespace ccmcp::constitution
