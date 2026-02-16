#include "ccmcp/interaction/redis_interaction_coordinator.h"

#include <stdexcept>
#include <sw/redis++/redis++.h>
#include <unordered_map>

namespace ccmcp::interaction {

namespace {

// Convert InteractionState enum to integer for Redis storage
int state_to_int(const domain::InteractionState state) {
  return static_cast<int>(state);
}

// Convert integer from Redis to InteractionState enum
domain::InteractionState int_to_state(const int value) {
  return static_cast<domain::InteractionState>(value);
}

// Convert InteractionEvent enum to integer for Redis storage
int event_to_int(const domain::InteractionEvent event) {
  return static_cast<int>(event);
}

// Convert integer from Redis to InteractionEvent enum
domain::InteractionEvent int_to_event(const int value) {
  return static_cast<domain::InteractionEvent>(value);
}

// Validate transition using domain logic
bool can_apply_event(const domain::InteractionState state, const domain::InteractionEvent event) {
  domain::Interaction temp{
      .interaction_id = core::InteractionId{"temp"},
      .contact_id = core::ContactId{"temp"},
      .opportunity_id = core::OpportunityId{"temp"},
      .state = state,
  };
  return temp.can_transition(event);
}

// Apply event to get next state (assumes can_apply_event already validated)
domain::InteractionState apply_event(const domain::InteractionState state,
                                     const domain::InteractionEvent event) {
  domain::Interaction temp{
      .interaction_id = core::InteractionId{"temp"},
      .contact_id = core::ContactId{"temp"},
      .opportunity_id = core::OpportunityId{"temp"},
      .state = state,
  };
  (void)temp.apply(event);  // Ignore return value (we already validated)
  return temp.state;
}

// Lua script for atomic transition application
// Args: state_key, idem_key, current_state (int), event (int), new_state (int)
// Returns: { outcome, before_state, after_state, transition_index }
//   outcome: 0=Applied, 1=AlreadyApplied, 2=Conflict, 3=NotFound
constexpr const char* kApplyTransitionScript = R"LUA(
local state_key = KEYS[1]
local idem_key = KEYS[2]
local event = tonumber(ARGV[1])
local new_state = tonumber(ARGV[2])

-- Check if interaction exists
if redis.call('EXISTS', state_key) == 0 then
  return {3, 0, 0, 0}  -- NotFound
end

-- Check idempotency: has this key been applied before?
if redis.call('EXISTS', idem_key) == 1 then
  local after_state = tonumber(redis.call('HGET', idem_key, 'after_state'))
  local transition_index = tonumber(redis.call('HGET', idem_key, 'transition_index'))
  return {1, after_state, after_state, transition_index}  -- AlreadyApplied
end

-- Read current state
local current_state = tonumber(redis.call('HGET', state_key, 'state'))
local current_index = tonumber(redis.call('HGET', state_key, 'transition_index'))

-- Apply transition
local next_index = current_index + 1

-- Update state
redis.call('HSET', state_key, 'state', new_state)
redis.call('HSET', state_key, 'transition_index', next_index)

-- Record idempotency receipt
redis.call('HSET', idem_key, 'after_state', new_state)
redis.call('HSET', idem_key, 'transition_index', next_index)
redis.call('HSET', idem_key, 'applied_event', event)

-- Return: outcome=Applied, before_state, after_state, transition_index
return {0, current_state, new_state, next_index}
)LUA";

}  // namespace

RedisInteractionCoordinator::RedisInteractionCoordinator(const std::string& redis_uri) {
  try {
    redis_ = std::make_unique<sw::redis::Redis>(redis_uri);
    // Test connection
    redis_->ping();
    // Load Lua scripts
    load_scripts();
  } catch (const std::exception& e) {
    throw std::runtime_error("Failed to connect to Redis: " + std::string(e.what()));
  }
}

RedisInteractionCoordinator::~RedisInteractionCoordinator() = default;

void RedisInteractionCoordinator::load_scripts() {
  // Load apply_transition script and cache SHA
  apply_transition_script_sha_ = redis_->script_load(kApplyTransitionScript);
}

TransitionResult RedisInteractionCoordinator::apply_transition(
    const core::InteractionId& interaction_id, const domain::InteractionEvent event,
    const std::string& idempotency_key) {
  try {
    const std::string state_key = "ccmcp:interaction:" + interaction_id.value + ":state";
    const std::string idem_key =
        "ccmcp:interaction:" + interaction_id.value + ":idem:" + idempotency_key;

    // First, check if interaction exists and get current state
    if (!redis_->exists(state_key)) {
      return TransitionResult{
          .outcome = TransitionOutcome::kNotFound,
          .before_state = domain::InteractionState::kDraft,
          .after_state = domain::InteractionState::kDraft,
          .transition_index = 0,
          .error_message = "Interaction not found: " + interaction_id.value,
      };
    }

    // Read current state to validate transition
    std::unordered_map<std::string, std::string> state_map;
    redis_->hgetall(state_key, std::inserter(state_map, state_map.end()));
    const int current_state_int = std::stoi(state_map.at("state"));
    const domain::InteractionState current_state = int_to_state(current_state_int);

    // Validate transition using domain logic
    if (!can_apply_event(current_state, event)) {
      const int64_t current_index = std::stoll(state_map.at("transition_index"));
      return TransitionResult{
          .outcome = TransitionOutcome::kInvalidTransition,
          .before_state = current_state,
          .after_state = current_state,
          .transition_index = current_index,
          .error_message = "Invalid transition from current state",
      };
    }

    // Compute new state using domain logic
    const domain::InteractionState new_state = apply_event(current_state, event);

    // Execute Lua script for atomic transition
    std::vector<std::string> keys = {state_key, idem_key};
    std::vector<std::string> args = {std::to_string(event_to_int(event)),
                                     std::to_string(state_to_int(new_state))};

    sw::redis::StringView script_sha{apply_transition_script_sha_};
    auto reply = redis_->evalsha<std::vector<long long>>(script_sha, keys.begin(), keys.end(),
                                                         args.begin(), args.end());

    // Parse result: { outcome, before_state, after_state, transition_index }
    const int outcome_int = static_cast<int>(reply[0]);
    const domain::InteractionState before_state = int_to_state(static_cast<int>(reply[1]));
    const domain::InteractionState after_state = int_to_state(static_cast<int>(reply[2]));
    const int64_t transition_index = reply[3];

    TransitionOutcome outcome;
    switch (outcome_int) {
      case 0:
        outcome = TransitionOutcome::kApplied;
        break;
      case 1:
        outcome = TransitionOutcome::kAlreadyApplied;
        break;
      case 2:
        outcome = TransitionOutcome::kConflict;
        break;
      case 3:
        outcome = TransitionOutcome::kNotFound;
        break;
      default:
        outcome = TransitionOutcome::kBackendError;
    }

    return TransitionResult{
        .outcome = outcome,
        .before_state = before_state,
        .after_state = after_state,
        .transition_index = transition_index,
        .error_message = "",
    };

  } catch (const std::exception& e) {
    return TransitionResult{
        .outcome = TransitionOutcome::kBackendError,
        .before_state = domain::InteractionState::kDraft,
        .after_state = domain::InteractionState::kDraft,
        .transition_index = 0,
        .error_message = "Redis error: " + std::string(e.what()),
    };
  }
}

std::optional<IInteractionCoordinator::StateInfo> RedisInteractionCoordinator::get_state(
    const core::InteractionId& interaction_id) const {
  try {
    const std::string state_key = "ccmcp:interaction:" + interaction_id.value + ":state";

    if (!redis_->exists(state_key)) {
      return std::nullopt;
    }

    std::unordered_map<std::string, std::string> state_map;
    redis_->hgetall(state_key, std::inserter(state_map, state_map.end()));
    const int state_int = std::stoi(state_map.at("state"));
    const int64_t transition_index = std::stoll(state_map.at("transition_index"));

    return StateInfo{
        .state = int_to_state(state_int),
        .transition_index = transition_index,
    };

  } catch (const std::exception& /*e*/) {
    return std::nullopt;
  }
}

bool RedisInteractionCoordinator::create_interaction(const core::InteractionId& interaction_id,
                                                     const core::ContactId& contact_id,
                                                     const core::OpportunityId& opportunity_id) {
  try {
    const std::string state_key = "ccmcp:interaction:" + interaction_id.value + ":state";

    // Check if already exists
    if (redis_->exists(state_key)) {
      return false;
    }

    // Create new interaction in kDraft state
    redis_->hset(state_key, "state",
                 std::to_string(state_to_int(domain::InteractionState::kDraft)));
    redis_->hset(state_key, "transition_index", "0");
    redis_->hset(state_key, "contact_id", contact_id.value);
    redis_->hset(state_key, "opportunity_id", opportunity_id.value);

    return true;

  } catch (const std::exception& /*e*/) {
    return false;
  }
}

}  // namespace ccmcp::interaction
