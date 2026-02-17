#pragma once

#include "ccmcp/domain/resume_token_ir.h"

#include <nlohmann/json.hpp>

#include <string>

namespace ccmcp::domain {

/// Serialize ResumeTokenIR to JSON (deterministic, sorted keys)
[[nodiscard]] nlohmann::json token_ir_to_json(const ResumeTokenIR& ir);

/// Deserialize ResumeTokenIR from JSON
[[nodiscard]] ResumeTokenIR token_ir_from_json(const nlohmann::json& j);

/// Serialize to stable JSON string (sorted keys, no whitespace)
[[nodiscard]] std::string token_ir_to_json_string(const ResumeTokenIR& ir);

}  // namespace ccmcp::domain
