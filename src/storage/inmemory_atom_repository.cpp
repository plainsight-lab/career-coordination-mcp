#include "ccmcp/storage/inmemory_atom_repository.h"

namespace ccmcp::storage {

void InMemoryAtomRepository::upsert(const domain::ExperienceAtom& atom) {
  atoms_[atom.atom_id] = atom;
}

std::optional<domain::ExperienceAtom> InMemoryAtomRepository::get(const core::AtomId& id) const {
  auto it = atoms_.find(id);
  if (it != atoms_.end()) {
    return it->second;
  }
  return std::nullopt;
}

std::vector<domain::ExperienceAtom> InMemoryAtomRepository::list_verified() const {
  std::vector<domain::ExperienceAtom> result;
  for (const auto& [id, atom] : atoms_) {
    if (atom.verified) {
      result.push_back(atom);
    }
  }
  return result;
}

std::vector<domain::ExperienceAtom> InMemoryAtomRepository::list_all() const {
  std::vector<domain::ExperienceAtom> result;
  for (const auto& [id, atom] : atoms_) {
    result.push_back(atom);
  }
  return result;
}

}  // namespace ccmcp::storage
