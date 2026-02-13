#pragma once

#include <string>
#include <vector>

#include "ccmcp/core/ids.h"
#include "ccmcp/domain/requirement.h"

namespace ccmcp::domain {

// Opportunity is a struct (not class) per C++ Core Guidelines C.2:
// Members can vary independently without violating invariants.
// This is a data aggregate representing a structured job posting.
struct Opportunity {
  core::OpportunityId opportunity_id;
  std::string company;
  std::string role_title;
  std::string source;
  std::vector<Requirement> requirements;
};

}  // namespace ccmcp::domain
