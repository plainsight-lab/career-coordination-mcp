#include "validate_match_report.h"

namespace ccmcp::mcp::handlers {

using json = nlohmann::json;

json handle_validate_match_report(const json& /*params*/, ServerContext& /*ctx*/) {
  json result;
  result["error"] = "validate_match_report not yet implemented";
  return result;
}

}  // namespace ccmcp::mcp::handlers
