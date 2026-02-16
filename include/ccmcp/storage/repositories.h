#pragma once

#include "ccmcp/domain/experience_atom.h"
#include "ccmcp/domain/interaction.h"
#include "ccmcp/domain/opportunity.h"

#include <optional>
#include <vector>

namespace ccmcp::storage {

// Repository interfaces isolate persistence for deterministic testing and auditing.
class IAtomRepository {
 public:
  virtual ~IAtomRepository() = default;
  virtual void upsert(const domain::ExperienceAtom& atom) = 0;
  [[nodiscard]] virtual std::optional<domain::ExperienceAtom> get(const core::AtomId& id) const = 0;
  [[nodiscard]] virtual std::vector<domain::ExperienceAtom> list_verified() const = 0;
  [[nodiscard]] virtual std::vector<domain::ExperienceAtom> list_all() const = 0;
};

class IOpportunityRepository {
 public:
  virtual ~IOpportunityRepository() = default;
  virtual void upsert(const domain::Opportunity& opportunity) = 0;
  [[nodiscard]] virtual std::optional<domain::Opportunity> get(
      const core::OpportunityId& id) const = 0;
  [[nodiscard]] virtual std::vector<domain::Opportunity> list_all() const = 0;
};

class IInteractionRepository {
 public:
  virtual ~IInteractionRepository() = default;
  virtual void upsert(const domain::Interaction& interaction) = 0;
  [[nodiscard]] virtual std::optional<domain::Interaction> get(
      const core::InteractionId& id) const = 0;
  [[nodiscard]] virtual std::vector<domain::Interaction> list_by_opportunity(
      const core::OpportunityId& id) const = 0;
  [[nodiscard]] virtual std::vector<domain::Interaction> list_all() const = 0;
};

}  // namespace ccmcp::storage
