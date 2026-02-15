#include "ccmcp/domain/requirement.h"

#include "ccmcp/core/normalization.h"

namespace ccmcp::domain {

core::Result<bool, std::string> Requirement::validate() const {
  // Check text non-empty
  if (text.empty()) {
    return core::Result<bool, std::string>::err("requirement text must not be empty");
  }

  // Tags must be normalized if present
  if (!tags.empty()) {
    auto normalized_tags = core::normalize_tags(tags);
    if (tags != normalized_tags) {
      return core::Result<bool, std::string>::err(
          "tags must be normalized (lowercase, sorted, deduplicated)");
    }
  }

  return core::Result<bool, std::string>::ok(true);
}

Requirement normalize_requirement(const Requirement& req) {
  Requirement normalized = req;

  // Trim text
  normalized.text = core::trim(req.text);

  // Normalize tags
  normalized.tags = core::normalize_tags(req.tags);

  return normalized;
}

}  // namespace ccmcp::domain
