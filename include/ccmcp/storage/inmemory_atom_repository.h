#pragma once

#include "ccmcp/storage/repositories.h"

#include <map>

namespace ccmcp::storage {

// InMemoryAtomRepository stores ExperienceAtoms in-memory using std::map.
// std::map guarantees deterministic iteration order (sorted by AtomId).
// Suitable for testing and v0.2 development; will be replaced with SQLite in later slices.
class InMemoryAtomRepository final : public IAtomRepository {
 public:
  void upsert(const domain::ExperienceAtom& atom) override;
  [[nodiscard]] std::optional<domain::ExperienceAtom> get(const core::AtomId& id) const override;
  [[nodiscard]] std::vector<domain::ExperienceAtom> list_verified() const override;
  [[nodiscard]] std::vector<domain::ExperienceAtom> list_all() const override;

 private:
  std::map<core::AtomId, domain::ExperienceAtom> atoms_;
};

}  // namespace ccmcp::storage
