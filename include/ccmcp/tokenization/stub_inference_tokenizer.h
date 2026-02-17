#pragma once

#include "ccmcp/tokenization/tokenization_provider.h"

#include <string>

namespace ccmcp::tokenization {

/// Stub inference tokenizer for testing (deterministic, no real ML)
/// Produces categorized tokens to simulate inference-assisted output
/// Uses deterministic rules based on patterns in the resume text
class StubInferenceTokenizer final : public ITokenizationProvider {
 public:
  StubInferenceTokenizer() = default;

  [[nodiscard]] domain::ResumeTokenIR tokenize(const std::string& resume_md,
                                               const std::string& source_hash) override;
};

}  // namespace ccmcp::tokenization
