#pragma once

#include <string>
#include <vector>

#include "ccmcp/constitution/rule.h"

namespace ccmcp::constitution {

struct Constitution {
  std::string constitution_id;
  std::string version;
  std::vector<Rule> rules;
};

}  // namespace ccmcp::constitution
