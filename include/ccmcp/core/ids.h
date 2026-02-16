#pragma once

#include "ccmcp/core/id_generator.h"

#include <string>

namespace ccmcp::core {

// Strong ID types following C++ Core Guidelines C.11 (Make concrete types regular).
// These are "vocabulary types" that prevent ID confusion and enable type-safe APIs.
// Using struct (not class) per C.2: members can vary independently, no invariants.

struct AtomId {
  std::string value;
  auto operator<=>(const AtomId&) const = default;  // C++20: generates ==, !=, <, <=, >, >=
};

struct OpportunityId {
  std::string value;
  auto operator<=>(const OpportunityId&) const = default;
};

struct ContactId {
  std::string value;
  auto operator<=>(const ContactId&) const = default;
};

struct InteractionId {
  std::string value;
  auto operator<=>(const InteractionId&) const = default;
};

struct ResumeId {
  std::string value;
  auto operator<=>(const ResumeId&) const = default;
};

struct TraceId {
  std::string value;
  auto operator<=>(const TraceId&) const = default;
};

// Thin helper functions with explicit ID generator injection.
// These are pure wrappers with no hidden policy - they delegate to IIdGenerator.
// Prefer these over the compatibility functions above.

inline AtomId new_atom_id(IIdGenerator& gen) {
  return AtomId{gen.next("atom")};
}

inline OpportunityId new_opportunity_id(IIdGenerator& gen) {
  return OpportunityId{gen.next("opp")};
}

inline ContactId new_contact_id(IIdGenerator& gen) {
  return ContactId{gen.next("contact")};
}

inline InteractionId new_interaction_id(IIdGenerator& gen) {
  return InteractionId{gen.next("interaction")};
}

inline ResumeId new_resume_id(IIdGenerator& gen) {
  return ResumeId{gen.next("resume")};
}

inline TraceId new_trace_id(IIdGenerator& gen) {
  return TraceId{gen.next("trace")};
}

}  // namespace ccmcp::core
