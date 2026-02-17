#pragma once

namespace ccmcp::constitution {

enum class ArtifactType {
  kUnknown,
  kMatchReport,
  kResumeTokenIR,
};

// ArtifactView is an abstract interface for typed artifact access.
// Validation rules operate on typed views, not serialized strings.
class ArtifactView {
 public:
  virtual ~ArtifactView() = default;

  // Returns the concrete artifact type for safe downcasting.
  [[nodiscard]] virtual ArtifactType type() const noexcept = 0;

 protected:
  ArtifactView() = default;
  ArtifactView(const ArtifactView&) = default;
  ArtifactView& operator=(const ArtifactView&) = default;
  ArtifactView(ArtifactView&&) = default;
  ArtifactView& operator=(ArtifactView&&) = default;
};

}  // namespace ccmcp::constitution
