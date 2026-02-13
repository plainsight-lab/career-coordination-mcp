#include "ccmcp/domain/interaction.h"

namespace ccmcp::domain {

bool Interaction::can_transition(const InteractionEvent event) const {
  switch (state) {
    case InteractionState::kDraft:
      return event == InteractionEvent::kPrepare || event == InteractionEvent::kClose;
    case InteractionState::kReady:
      return event == InteractionEvent::kSend || event == InteractionEvent::kClose;
    case InteractionState::kSent:
      return event == InteractionEvent::kReceiveReply || event == InteractionEvent::kClose;
    case InteractionState::kResponded:
      return event == InteractionEvent::kClose;
    case InteractionState::kClosed:
      return false;
  }
  return false;
}

bool Interaction::apply(const InteractionEvent event) {
  if (!can_transition(event)) {
    return false;
  }

  switch (event) {
    case InteractionEvent::kPrepare:
      state = InteractionState::kReady;
      return true;
    case InteractionEvent::kSend:
      state = InteractionState::kSent;
      return true;
    case InteractionEvent::kReceiveReply:
      state = InteractionState::kResponded;
      return true;
    case InteractionEvent::kClose:
      state = InteractionState::kClosed;
      return true;
  }
  return false;
}

}  // namespace ccmcp::domain
