#pragma once

#include <string>
#include <vector>

#include "ccmcp/core/ids.h"

namespace ccmcp::domain {

struct ExperienceAtom {
  core::AtomId atom_id;
  std::string domain;
  std::string title;
  std::string claim;
  std::vector<std::string> tags;
  bool verified{false};

  [[nodiscard]] bool verify() const;
};

}  // namespace ccmcp::domain
