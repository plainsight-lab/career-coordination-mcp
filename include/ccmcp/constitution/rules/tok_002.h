#pragma once

#include "ccmcp/constitution/constitutional_rule.h"

namespace ccmcp::constitution {

// TOK-002: Tokens must be lowercase ASCII with length constraints (FAIL severity)
// Validates token format:
// - All tokens are lowercase ASCII (a-z, 0-9)
// - Token length >= 2 characters
// - No whitespace or special characters
// Rationale: Ensures deterministic token matching
class Tok002 final : public ConstitutionalRule {
 public:
  Tok002() = default;

  [[nodiscard]] std::string_view rule_id() const noexcept override { return "TOK-002"; }
  [[nodiscard]] std::string_view version() const noexcept override { return "0.3.0"; }
  [[nodiscard]] std::string_view description() const noexcept override {
    return "Tokens must be lowercase ASCII with length constraints";
  }

  [[nodiscard]] std::vector<Finding> Validate(const ArtifactEnvelope& envelope,
                                              const ValidationContext& context) const override;

 private:
  static constexpr size_t kMinTokenLength = 2;
};

}  // namespace ccmcp::constitution
