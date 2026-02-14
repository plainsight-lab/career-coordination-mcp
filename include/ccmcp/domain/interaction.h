#pragma once

#include "ccmcp/core/ids.h"

#include <string>

namespace ccmcp::domain {

enum class InteractionState {
  kDraft,
  kReady,
  kSent,
  kResponded,
  kClosed,
};

enum class InteractionEvent {
  kPrepare,
  kSend,
  kReceiveReply,
  kClose,
};

struct Interaction {
  core::InteractionId interaction_id;
  core::ContactId contact_id;
  core::OpportunityId opportunity_id;
  InteractionState state{InteractionState::kDraft};

  [[nodiscard]] bool can_transition(InteractionEvent event) const;
  [[nodiscard]] bool apply(InteractionEvent event);
};

}  // namespace ccmcp::domain
