#pragma once

#include "ccmcp/constitution/rule.h"

#include <string>
#include <vector>

namespace ccmcp::constitution {

struct Constitution {
  std::string constitution_id;
  std::string version;
  std::vector<Rule> rules;
};

}  // namespace ccmcp::constitution
