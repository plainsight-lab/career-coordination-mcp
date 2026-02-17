#pragma once

#include "ccmcp/domain/resume_token_ir.h"

#include <string>

namespace ccmcp::tokenization {

/// Interface for resume tokenization providers
class ITokenizationProvider {
 public:
  virtual ~ITokenizationProvider() = default;

  /// Tokenize a resume markdown and produce Token IR
  /// @param resume_md Canonical resume markdown
  /// @param source_hash Resume hash for provenance binding
  /// @return Token IR with tokens and optional spans
  [[nodiscard]] virtual domain::ResumeTokenIR tokenize(const std::string& resume_md,
                                                       const std::string& source_hash) = 0;
};

}  // namespace ccmcp::tokenization
