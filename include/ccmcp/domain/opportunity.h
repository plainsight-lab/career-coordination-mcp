#pragma once

#include <string>
#include <vector>

#include "ccmcp/core/ids.h"
#include "ccmcp/domain/requirement.h"

namespace ccmcp::domain {

struct Opportunity {
  core::OpportunityId opportunity_id;
  std::string company;
  std::string role_title;
  std::string source;
  std::vector<Requirement> requirements;
};

}  // namespace ccmcp::domain
