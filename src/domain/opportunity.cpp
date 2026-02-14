#include "ccmcp/domain/opportunity.h"

#include "ccmcp/core/normalization.h"

namespace ccmcp::domain {

core::Result<bool, std::string> Opportunity::validate() const {
  // Check opportunity_id non-empty
  if (opportunity_id.value.empty()) {
    return core::Result<bool, std::string>::err("opportunity_id must not be empty");
  }

  // Check company non-empty
  if (company.empty()) {
    return core::Result<bool, std::string>::err("company must not be empty");
  }

  // Check role_title non-empty
  if (role_title.empty()) {
    return core::Result<bool, std::string>::err("role_title must not be empty");
  }

  // Validate all requirements
  for (const auto& req : requirements) {
    auto req_result = req.validate();
    if (!req_result.has_value()) {
      return core::Result<bool, std::string>::err("invalid requirement: " + req_result.error());
    }
  }

  return core::Result<bool, std::string>::ok(true);
}

Opportunity normalize_opportunity(const Opportunity& opp) {
  Opportunity normalized = opp;

  // Trim company, role_title, source
  normalized.company = core::trim(opp.company);
  normalized.role_title = core::trim(opp.role_title);
  normalized.source = core::trim(opp.source);

  // Normalize all requirements (preserving order)
  normalized.requirements.clear();
  normalized.requirements.reserve(opp.requirements.size());
  for (const auto& req : opp.requirements) {
    normalized.requirements.push_back(normalize_requirement(req));
  }

  return normalized;
}

}  // namespace ccmcp::domain
