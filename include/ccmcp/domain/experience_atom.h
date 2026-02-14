#pragma once

#include "ccmcp/core/ids.h"

#include <string>
#include <vector>

namespace ccmcp::domain {

// ExperienceAtom is a struct (not class) per C++ Core Guidelines C.2:
// Members can vary independently; no invariants enforced on their relationships.
// This is an immutable capability fact - members are set during construction and
// can be read/modified independently without violating any structural constraints.
struct ExperienceAtom {
  core::AtomId atom_id;
  std::string domain;
  std::string title;
  std::string claim;
  std::vector<std::string> tags;
  bool verified{false};

  // Con.2: Member function is const - it doesn't modify observable state.
  [[nodiscard]] bool verify() const;
};

}  // namespace ccmcp::domain
