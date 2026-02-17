#pragma once

#include "ccmcp/core/clock.h"
#include "ccmcp/core/id_generator.h"
#include "ccmcp/core/services.h"
#include "ccmcp/interaction/interaction_coordinator.h"

#include "config.h"

namespace ccmcp::mcp {

struct ServerContext {
  core::Services& services;                           // NOLINT(readability-identifier-naming)
  interaction::IInteractionCoordinator& coordinator;  // NOLINT(readability-identifier-naming)
  core::IIdGenerator& id_gen;                         // NOLINT(readability-identifier-naming)
  core::IClock& clock;                                // NOLINT(readability-identifier-naming)
  McpServerConfig& config;                            // NOLINT(readability-identifier-naming)
};

}  // namespace ccmcp::mcp
