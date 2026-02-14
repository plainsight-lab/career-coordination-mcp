#pragma once

#include "ccmcp/core/ids.h"
#include "ccmcp/core/result.h"
#include "ccmcp/domain/requirement.h"

#include <string>
#include <vector>

namespace ccmcp::domain {

// Opportunity represents a structured job posting.
// v0.1 Schema (LOCKED):
// - opportunity_id: Non-empty unique identifier
// - company: Non-empty company name, trimmed
// - role_title: Non-empty role title, trimmed
// - requirements: List of requirements (preserves input order)
// - source: Optional source reference, trimmed
struct Opportunity {
  core::OpportunityId opportunity_id;
  std::string company;
  std::string role_title;
  std::vector<Requirement> requirements;
  std::string source;

  // validate checks schema invariants.
  // Returns ok(true) if valid, err(message) if invalid.
  core::Result<bool, std::string> validate() const;
};

// normalize_opportunity produces a normalized copy.
// - Trims company, role_title, source
// - Normalizes all requirements
// - Preserves requirements order (does not sort)
Opportunity normalize_opportunity(const Opportunity& opp);

}  // namespace ccmcp::domain
