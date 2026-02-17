#pragma once

#include "ccmcp/core/ids.h"
#include "ccmcp/domain/resume_token_ir.h"

#include <optional>
#include <string>
#include <vector>

namespace ccmcp::tokenization {

// IResumeTokenStore: Storage interface for Resume Token IR
// Responsibilities:
// - Persist token IR with provenance binding (resume_id)
// - Support retrieval by token_ir_id or resume_id
// - Guarantee deterministic ordering
//
// Implementations must preserve:
// - JSON serialization format stability
// - Deterministic list_all() ordering (ORDER BY token_ir_id)
class IResumeTokenStore {
 public:
  virtual ~IResumeTokenStore() = default;

  // Upsert token IR (creates or replaces)
  // token_ir_id is derived from resume_id + tokenizer metadata
  virtual void upsert(const std::string& token_ir_id, const core::ResumeId& resume_id,
                      const domain::ResumeTokenIR& token_ir) = 0;

  // Get token IR by ID
  [[nodiscard]] virtual std::optional<domain::ResumeTokenIR> get(
      const std::string& token_ir_id) const = 0;

  // Get token IR by resume ID
  [[nodiscard]] virtual std::optional<domain::ResumeTokenIR> get_by_resume(
      const core::ResumeId& resume_id) const = 0;

  // List all token IRs (deterministic order: ORDER BY token_ir_id)
  [[nodiscard]] virtual std::vector<domain::ResumeTokenIR> list_all() const = 0;
};

}  // namespace ccmcp::tokenization
