#include "interaction_apply_event.h"

#include "ccmcp/app/app_service.h"
#include "ccmcp/core/ids.h"
#include "ccmcp/domain/interaction.h"
#include "ccmcp/interaction/interaction_coordinator.h"

#include <exception>
#include <stdexcept>
#include <string>

namespace ccmcp::mcp::handlers {

using json = nlohmann::json;

json handle_interaction_apply_event(const json& params, ServerContext& ctx) {
  try {
    app::InteractionTransitionRequest request{
        .interaction_id = core::InteractionId{params.at("interaction_id").get<std::string>()},
        .event =
            [&]() {
              std::string event_str = params.at("event");
              if (event_str == "Prepare") {
                return domain::InteractionEvent::kPrepare;
              }
              if (event_str == "Send") {
                return domain::InteractionEvent::kSend;
              }
              if (event_str == "ReceiveReply") {
                return domain::InteractionEvent::kReceiveReply;
              }
              if (event_str == "Close") {
                return domain::InteractionEvent::kClose;
              }
              throw std::invalid_argument("Unknown event: " + event_str);
            }(),
        .idempotency_key = params.at("idempotency_key").get<std::string>(),
    };

    if (params.contains("trace_id")) {
      request.trace_id = params["trace_id"];
    }

    auto response = app::run_interaction_transition(request, ctx.coordinator, ctx.services,
                                                    ctx.id_gen, ctx.clock);

    json result;
    result["trace_id"] = response.trace_id;
    result["result"] = {
        {"outcome",
         [&]() {
           switch (response.result.outcome) {
             case interaction::TransitionOutcome::kApplied:
               return "applied";
             case interaction::TransitionOutcome::kAlreadyApplied:
               return "already_applied";
             case interaction::TransitionOutcome::kConflict:
               return "conflict";
             case interaction::TransitionOutcome::kNotFound:
               return "not_found";
             case interaction::TransitionOutcome::kInvalidTransition:
               return "invalid_transition";
             case interaction::TransitionOutcome::kBackendError:
               return "backend_error";
             default:
               return "unknown";
           }
         }()},
        {"before_state", static_cast<int>(response.result.before_state)},
        {"after_state", static_cast<int>(response.result.after_state)},
        {"transition_index", response.result.transition_index},
    };

    return result;

  } catch (const std::exception& e) {
    json error_result;
    error_result["error"] = e.what();
    return error_result;
  }
}

}  // namespace ccmcp::mcp::handlers
