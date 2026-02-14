#include "ccmcp/domain/experience_atom.h"

#include "ccmcp/core/normalization.h"

namespace ccmcp::domain {

bool ExperienceAtom::verify() const {
  // v0.1: Simple pass-through of verified flag.
  // Future: Evidence checking, attestation logic.
  return verified;
}

core::Result<bool, std::string> ExperienceAtom::validate() const {
  // Check atom_id non-empty
  if (atom_id.value.empty()) {
    return core::Result<bool, std::string>::err("atom_id must not be empty");
  }

  // Check claim non-empty
  if (claim.empty()) {
    return core::Result<bool, std::string>::err("claim must not be empty");
  }

  // Tags must be normalized (lowercase, sorted, no duplicates)
  // We verify by checking if re-normalizing produces same result
  auto normalized_tags = core::normalize_tags(tags);
  if (tags != normalized_tags) {
    return core::Result<bool, std::string>::err("tags must be normalized (lowercase, sorted, deduplicated)");
  }

  // Domain must be normalized (lowercase)
  auto normalized_domain = core::normalize_ascii_lower(domain);
  if (domain != normalized_domain) {
    return core::Result<bool, std::string>::err("domain must be normalized (lowercase)");
  }

  return core::Result<bool, std::string>::ok(true);
}

ExperienceAtom normalize_atom(const ExperienceAtom& atom) {
  ExperienceAtom normalized = atom;

  // Normalize domain to lowercase
  normalized.domain = core::normalize_ascii_lower(core::trim(atom.domain));

  // Trim title and claim
  normalized.title = core::trim(atom.title);
  normalized.claim = core::trim(atom.claim);

  // Normalize tags (lowercase, sorted, deduplicated)
  normalized.tags = core::normalize_tags(atom.tags);

  // Trim evidence refs
  normalized.evidence_refs.clear();
  normalized.evidence_refs.reserve(atom.evidence_refs.size());
  for (const auto& ref : atom.evidence_refs) {
    auto trimmed = core::trim(ref);
    if (!trimmed.empty()) {
      normalized.evidence_refs.push_back(std::move(trimmed));
    }
  }

  return normalized;
}

}  // namespace ccmcp::domain
