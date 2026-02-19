#pragma once

#include "ccmcp/constitution/constitution.h"
#include "ccmcp/constitution/override_request.h"
#include "ccmcp/constitution/validation_report.h"

#include <optional>

namespace ccmcp::constitution {

// ValidationEngine is a class (not struct) per C++ Core Guidelines C.2:
// It has private members and maintains an invariant (constitution_ must be valid).
// This encapsulation ensures the constitution cannot be corrupted after construction.
class ValidationEngine {
 public:
  explicit ValidationEngine(Constitution constitution);

  // Con.2: validate() is const - it performs validation without modifying engine state.
  // The engine is immutable after construction for deterministic, thread-safe validation.
  //
  // If override is provided and matches a BLOCK finding (rule_id + payload_hash), the
  // report status is set to kOverridden. BLOCK findings remain in the findings list so
  // the audit trail is preserved. All other behavior is unchanged when override is nullopt.
  //
  // payload_hash must equal stable_hash64_hex(envelope.artifact_id); callers are
  // responsible for binding the override to the current artifact before calling validate().
  [[nodiscard]] ValidationReport validate(
      const ArtifactEnvelope& envelope, const ValidationContext& context,
      const std::optional<ConstitutionOverrideRequest>& override = std::nullopt) const;

 private:
  Constitution constitution_;
};

Constitution make_default_constitution();

}  // namespace ccmcp::constitution
