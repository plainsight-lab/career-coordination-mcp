#pragma once

#include "ccmcp/constitution/artifact_view.h"
#include "ccmcp/domain/match_report.h"

namespace ccmcp::constitution {

// MatchReportView provides typed read-only access to a MatchReport for validation.
// This is a non-owning view; the caller must ensure the MatchReport outlives the view.
//
// C++ Core Guidelines C.12 compliance: Reference member with deleted copy/move.
// A view is semantically non-copyable - it represents a temporary reference to
// an external object and should not be copied or moved.
class MatchReportView final : public ArtifactView {
 public:
  explicit MatchReportView(const domain::MatchReport& report) : report_(report) {}

  // Non-copyable, non-movable (view semantics - C.12 compliance)
  MatchReportView(const MatchReportView&) = delete;
  MatchReportView& operator=(const MatchReportView&) = delete;
  MatchReportView(MatchReportView&&) = delete;
  MatchReportView& operator=(MatchReportView&&) = delete;

  [[nodiscard]] ArtifactType type() const noexcept override { return ArtifactType::kMatchReport; }

  [[nodiscard]] const domain::MatchReport& report() const noexcept { return report_; }

 private:
  const domain::MatchReport& report_;
};

}  // namespace ccmcp::constitution
