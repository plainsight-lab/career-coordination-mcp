#pragma once

#include "ccmcp/constitution/artifact_view.h"
#include "ccmcp/domain/resume_token_ir.h"

#include <string>

namespace ccmcp::constitution {

/// ArtifactView for Resume Token IR validation
class TokenIRArtifactView final : public ArtifactView {
 public:
  explicit TokenIRArtifactView(const domain::ResumeTokenIR& token_ir,
                               const std::string& canonical_resume_hash,
                               const std::string& canonical_resume_text);

  [[nodiscard]] ArtifactType type() const noexcept override { return ArtifactType::kResumeTokenIR; }

  [[nodiscard]] const domain::ResumeTokenIR& token_ir() const noexcept { return token_ir_; }

  [[nodiscard]] const std::string& canonical_resume_hash() const noexcept {
    return canonical_resume_hash_;
  }

  [[nodiscard]] const std::string& canonical_resume_text() const noexcept {
    return canonical_resume_text_;
  }

 private:
  const domain::ResumeTokenIR& token_ir_;
  const std::string& canonical_resume_hash_;
  const std::string& canonical_resume_text_;
};

}  // namespace ccmcp::constitution
