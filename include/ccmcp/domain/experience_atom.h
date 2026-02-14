#pragma once

#include "ccmcp/core/ids.h"
#include "ccmcp/core/result.h"

#include <string>
#include <vector>

namespace ccmcp::domain {

// ExperienceAtom represents an immutable, verifiable capability fact.
// This is a struct (not class) per C++ Core Guidelines C.2: members can vary
// independently. Invariants are enforced via validate() and normalize().
//
// v0.1 Schema (LOCKED):
// - atom_id: Non-empty unique identifier
// - domain: Normalized lowercase ASCII, trimmed
// - title: Free-form text, trimmed
// - claim: Non-empty capability statement, trimmed
// - tags: Normalized (lowercase, sorted, deduplicated, min length 2)
// - verified: Boolean attestation flag
// - evidence_refs: List of evidence pointers (URLs, doc refs), trimmed
struct ExperienceAtom {
  core::AtomId atom_id;
  std::string domain;
  std::string title;
  std::string claim;
  std::vector<std::string> tags;
  bool verified{false};
  std::vector<std::string> evidence_refs;

  // Con.2: Member function is const - it doesn't modify observable state.
  [[nodiscard]] bool verify() const;

  // validate checks schema invariants.
  // Returns ok(true) if valid, err(message) if invalid.
  [[nodiscard]] core::Result<bool, std::string> validate() const;
};

// normalize_atom produces a normalized copy of an atom.
// - Normalizes domain to lowercase ASCII
// - Trims title, claim, and evidence_refs
// - Normalizes tags (lowercase, sorted, deduplicated)
// - Preserves atom_id and verified flag
[[nodiscard]] ExperienceAtom normalize_atom(const ExperienceAtom& atom);

}  // namespace ccmcp::domain
