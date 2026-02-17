#pragma once

#include "ccmcp/constitution/constitutional_rule.h"

namespace ccmcp::constitution {

// TOK-005: Token volume must not exceed threshold (WARN severity)
// Validates reasonable token count:
// - Total token count across all categories <= threshold (default: 500)
// - Per-category token count <= threshold (default: 200)
// Rationale: Warns on excessive tokenization that may indicate quality issues
class Tok005 final : public ConstitutionalRule {
 public:
  Tok005() = default;

  [[nodiscard]] std::string_view rule_id() const noexcept override { return "TOK-005"; }
  [[nodiscard]] std::string_view version() const noexcept override { return "0.3.0"; }
  [[nodiscard]] std::string_view description() const noexcept override {
    return "Token volume must not exceed threshold";
  }

  [[nodiscard]] std::vector<Finding> Validate(const ArtifactEnvelope& envelope,
                                              const ValidationContext& context) const override;

 private:
  static constexpr size_t kMaxTotalTokens = 500;
  static constexpr size_t kMaxCategoryTokens = 200;
};

}  // namespace ccmcp::constitution
