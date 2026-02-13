#pragma once

#include "ccmcp/constitution/constitution.h"
#include "ccmcp/constitution/validation_report.h"

namespace ccmcp::constitution {

// ValidationEngine is a class (not struct) per C++ Core Guidelines C.2:
// It has private members and maintains an invariant (constitution_ must be valid).
// This encapsulation ensures the constitution cannot be corrupted after construction.
class ValidationEngine {
 public:
  explicit ValidationEngine(Constitution constitution);

  // Con.2: validate() is const - it performs validation without modifying engine state.
  // The engine is immutable after construction for deterministic, thread-safe validation.
  [[nodiscard]] ValidationReport validate(
      const ArtifactEnvelope& envelope,
      const ValidationContext& context) const;

 private:
  Constitution constitution_;
};

Constitution make_default_constitution();

}  // namespace ccmcp::constitution
