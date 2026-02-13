#pragma once

#include "ccmcp/constitution/constitution.h"
#include "ccmcp/constitution/validation_report.h"

namespace ccmcp::constitution {

class ValidationEngine {
 public:
  explicit ValidationEngine(Constitution constitution);

  [[nodiscard]] ValidationReport validate(
      const ArtifactEnvelope& envelope,
      const ValidationContext& context) const;

 private:
  Constitution constitution_;
};

Constitution make_default_constitution();

}  // namespace ccmcp::constitution
