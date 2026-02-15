#pragma once

#include "ccmcp/constitution/constitutional_rule.h"

#include <memory>
#include <string>
#include <vector>

namespace ccmcp::constitution {

// Constitution holds an ordered collection of polymorphic validation rules.
// Rule evaluation order is deterministic (preserves vector order).
struct Constitution {
  std::string constitution_id;
  std::string version;
  std::vector<std::unique_ptr<const ConstitutionalRule>> rules;
};

}  // namespace ccmcp::constitution
