#pragma once

#include <string>

namespace ccmcp::domain {

enum class RequirementType {
  kSkill,
  kDomain,
  kConstraint,
  kOther,
};

struct Requirement {
  std::string requirement_id;
  RequirementType type{RequirementType::kOther};
  std::string text;
};

}  // namespace ccmcp::domain
