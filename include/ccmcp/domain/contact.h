#pragma once

#include <string>
#include <vector>

#include "ccmcp/core/ids.h"

namespace ccmcp::domain {

enum class RelationshipState {
  kUnknown,
  kWarm,
  kCold,
  kRecruiter,
  kHiringManager,
  kDoNotContact,
};

struct Contact {
  core::ContactId contact_id;
  std::string name;
  std::string org;
  std::vector<std::string> identity_keys;
  RelationshipState relationship_state{RelationshipState::kUnknown};
};

}  // namespace ccmcp::domain
