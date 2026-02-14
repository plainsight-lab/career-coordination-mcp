#pragma once

#include "ccmcp/core/result.h"

#include <string>
#include <vector>

namespace ccmcp::domain {

enum class RequirementType {
  kSkill,
  kDomain,
  kConstraint,
  kOther,
};

// Requirement represents a single job requirement with optional categorization.
// v0.1 Schema (LOCKED):
// - text: Non-empty requirement description, trimmed
// - tags: Normalized tags (lowercase, sorted, deduplicated)
// - required: Whether this is mandatory (true) or nice-to-have (false)
struct Requirement {
  std::string text;
  std::vector<std::string> tags;
  bool required{true};

  // validate checks schema invariants.
  // Returns ok(true) if valid, err(message) if invalid.
  core::Result<bool, std::string> validate() const;
};

// normalize_requirement produces a normalized copy.
// - Trims text
// - Normalizes tags (lowercase, sorted, deduplicated)
Requirement normalize_requirement(const Requirement& req);

}  // namespace ccmcp::domain
