#pragma once

#include "ccmcp/domain/experience_atom.h"
#include "ccmcp/domain/opportunity.h"

#include <vector>

namespace ccmcp::storage {

// Repository interfaces isolate persistence for deterministic testing and auditing.
class IAtomRepository {
 public:
  virtual ~IAtomRepository() = default;
  virtual std::vector<domain::ExperienceAtom> list_all() const = 0;
};

class IOpportunityRepository {
 public:
  virtual ~IOpportunityRepository() = default;
  virtual std::vector<domain::Opportunity> list_all() const = 0;
};

}  // namespace ccmcp::storage
