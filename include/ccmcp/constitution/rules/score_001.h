#pragma once

#include "ccmcp/constitution/constitutional_rule.h"

namespace ccmcp::constitution {

// SCORE-001: Warn on degenerate scoring (WARN severity)
// Validates:
// - overall_score == 0.0 AND requirements non-empty → emit warning
class Score001 final : public ConstitutionalRule {
 public:
  Score001() = default;

  [[nodiscard]] std::string_view rule_id() const noexcept override { return "SCORE-001"; }
  [[nodiscard]] std::string_view version() const noexcept override { return "0.1.0"; }
  [[nodiscard]] std::string_view description() const noexcept override {
    return "Warn on degenerate scoring";
  }

  [[nodiscard]] std::vector<Finding> Validate(const ArtifactEnvelope& envelope,
                                              const ValidationContext& context) const override;
};

}  // namespace ccmcp::constitution
