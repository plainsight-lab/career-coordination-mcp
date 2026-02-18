#pragma once

#include "ccmcp/core/clock.h"
#include "ccmcp/core/id_generator.h"
#include "ccmcp/core/services.h"
#include "ccmcp/indexing/index_run_store.h"
#include "ccmcp/ingest/resume_ingestor.h"
#include "ccmcp/ingest/resume_store.h"
#include "ccmcp/interaction/interaction_coordinator.h"
#include "ccmcp/storage/decision_store.h"

#include "config.h"

namespace ccmcp::mcp {

// ServerContext holds all process-lifetime service references passed to every tool handler.
// All references must remain valid for the lifetime of run_server_loop().
struct ServerContext {
  core::Services& services;                           // NOLINT(readability-identifier-naming)
  interaction::IInteractionCoordinator& coordinator;  // NOLINT(readability-identifier-naming)
  ingest::IResumeIngestor& ingestor;                  // NOLINT(readability-identifier-naming)
  ingest::IResumeStore& resume_store;                 // NOLINT(readability-identifier-naming)
  indexing::IIndexRunStore& index_run_store;          // NOLINT(readability-identifier-naming)
  storage::IDecisionStore& decision_store;            // NOLINT(readability-identifier-naming)
  core::IIdGenerator& id_gen;                         // NOLINT(readability-identifier-naming)
  core::IClock& clock;                                // NOLINT(readability-identifier-naming)
  McpServerConfig& config;                            // NOLINT(readability-identifier-naming)
};

}  // namespace ccmcp::mcp
