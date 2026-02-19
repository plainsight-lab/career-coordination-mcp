#pragma once

#include "ccmcp/constitution/override_request.h"
#include "ccmcp/constitution/validation_engine.h"
#include "ccmcp/constitution/validation_report.h"
#include "ccmcp/core/clock.h"
#include "ccmcp/core/id_generator.h"
#include "ccmcp/core/ids.h"
#include "ccmcp/core/services.h"
#include "ccmcp/domain/decision_record.h"
#include "ccmcp/domain/interaction.h"
#include "ccmcp/domain/match_report.h"
#include "ccmcp/domain/opportunity.h"
#include "ccmcp/indexing/index_run_store.h"
#include "ccmcp/ingest/resume_ingestor.h"
#include "ccmcp/ingest/resume_store.h"
#include "ccmcp/interaction/interaction_coordinator.h"
#include "ccmcp/matching/matcher.h"
#include "ccmcp/storage/audit_event.h"
#include "ccmcp/storage/decision_store.h"

#include <optional>
#include <string>
#include <vector>

namespace ccmcp::app {

// ────────────────────────────────────────────────────────────────
// Match Pipeline
// ────────────────────────────────────────────────────────────────

struct MatchPipelineRequest {
  // Opportunity to match against (provide either opportunity OR opportunity_id)
  std::optional<domain::Opportunity> opportunity;     // NOLINT(readability-identifier-naming)
  std::optional<core::OpportunityId> opportunity_id;  // NOLINT(readability-identifier-naming)

  // Atoms to match (provide either atoms OR atom_ids, or neither for all verified)
  std::optional<std::vector<domain::ExperienceAtom>>
      atoms;                                          // NOLINT(readability-identifier-naming)
  std::optional<std::vector<core::AtomId>> atom_ids;  // NOLINT(readability-identifier-naming)

  // Matching configuration
  matching::MatchingStrategy strategy{
      matching::MatchingStrategy::
          kDeterministicLexicalV01};  // NOLINT(readability-identifier-naming)
  size_t k_lex{25};                   // NOLINT(readability-identifier-naming)
  size_t k_emb{25};                   // NOLINT(readability-identifier-naming)

  // Optional trace_id (if not provided, will be generated)
  std::optional<std::string> trace_id;  // NOLINT(readability-identifier-naming)

  // Optional resume context (for audit trail traceability only — does not alter matching)
  std::optional<core::ResumeId> resume_id;  // NOLINT(readability-identifier-naming)

  // Optional constitutional override: applied in run_validation_pipeline() to convert
  // a BLOCK finding to kOverridden. payload_hash is bound to the artifact by the pipeline.
  // Fails silently (no override applied) if rule_id or payload_hash does not match.
  std::optional<constitution::ConstitutionOverrideRequest>
      override_request;  // NOLINT(readability-identifier-naming)
};

struct MatchPipelineResponse {
  std::string trace_id;                              // NOLINT(readability-identifier-naming)
  domain::MatchReport match_report;                  // NOLINT(readability-identifier-naming)
  constitution::ValidationReport validation_report;  // NOLINT(readability-identifier-naming)
};

// Run matching + validation pipeline
// Emits audit events: RunStarted, MatchCompleted, ValidationCompleted, RunCompleted
[[nodiscard]] MatchPipelineResponse run_match_pipeline(const MatchPipelineRequest& req,
                                                       core::Services& services,
                                                       core::IIdGenerator& id_gen,
                                                       core::IClock& clock);

// ────────────────────────────────────────────────────────────────
// Validation Pipeline (standalone)
// ────────────────────────────────────────────────────────────────

// Run validation only on an existing match report.
// Emits audit event: ValidationCompleted.
// If override is provided, run_validation_pipeline() binds payload_hash to the artifact
// (stable_hash64_hex(envelope.artifact_id)) before calling validate(). If the override
// matches a BLOCK finding, status becomes kOverridden and ConstitutionOverrideApplied
// audit event is emitted after ValidationCompleted.
[[nodiscard]] constitution::ValidationReport run_validation_pipeline(
    const domain::MatchReport& report, core::Services& services, core::IIdGenerator& id_gen,
    core::IClock& clock, const std::string& trace_id,
    std::optional<constitution::ConstitutionOverrideRequest> override = std::nullopt);

// ────────────────────────────────────────────────────────────────
// Interaction Transition
// ────────────────────────────────────────────────────────────────

struct InteractionTransitionRequest {
  core::InteractionId interaction_id;  // NOLINT(readability-identifier-naming)
  domain::InteractionEvent event;      // NOLINT(readability-identifier-naming)
  std::string idempotency_key;         // NOLINT(readability-identifier-naming)

  // Optional trace_id (if not provided, will be generated)
  std::optional<std::string> trace_id;  // NOLINT(readability-identifier-naming)
};

struct InteractionTransitionResponse {
  std::string trace_id;                  // NOLINT(readability-identifier-naming)
  interaction::TransitionResult result;  // NOLINT(readability-identifier-naming)
};

// Apply interaction state transition atomically
// Emits audit events: InteractionTransitionAttempted, InteractionTransitionCompleted/Rejected
[[nodiscard]] InteractionTransitionResponse run_interaction_transition(
    const InteractionTransitionRequest& req, interaction::IInteractionCoordinator& coordinator,
    core::Services& services, core::IIdGenerator& id_gen, core::IClock& clock);

// ────────────────────────────────────────────────────────────────
// Audit Trace
// ────────────────────────────────────────────────────────────────

// Fetch all audit events for a given trace_id
[[nodiscard]] std::vector<storage::AuditEvent> fetch_audit_trace(const std::string& trace_id,
                                                                 core::Services& services);

// ────────────────────────────────────────────────────────────────
// Decision Records
// ────────────────────────────────────────────────────────────────

// Build and persist a DecisionRecord from a completed match pipeline response.
// Emits audit event: DecisionRecorded
// Returns the generated decision_id.
[[nodiscard]] std::string record_match_decision(const MatchPipelineResponse& pipeline_response,
                                                storage::IDecisionStore& decision_store,
                                                core::Services& services,
                                                core::IIdGenerator& id_gen, core::IClock& clock);

// Fetch a single decision record by decision_id. Returns nullopt if not found.
[[nodiscard]] std::optional<domain::DecisionRecord> fetch_decision(
    const std::string& decision_id, storage::IDecisionStore& decision_store);

// List all decision records for a trace, ordered by decision_id ascending.
[[nodiscard]] std::vector<domain::DecisionRecord> list_decisions_by_trace(
    const std::string& trace_id, storage::IDecisionStore& decision_store);

// ────────────────────────────────────────────────────────────────
// Ingest Resume Pipeline
// ────────────────────────────────────────────────────────────────

struct IngestResumePipelineRequest {
  std::string input_path;               // NOLINT(readability-identifier-naming)
  bool persist{true};                   // NOLINT(readability-identifier-naming)
  std::optional<std::string> trace_id;  // NOLINT(readability-identifier-naming)
};

struct IngestResumePipelineResponse {
  std::string resume_id;    // NOLINT(readability-identifier-naming)
  std::string resume_hash;  // NOLINT(readability-identifier-naming)
  std::string source_hash;  // NOLINT(readability-identifier-naming)
  std::string trace_id;     // NOLINT(readability-identifier-naming)
};

// Ingest a resume file, optionally persist it, and emit audit events.
// Emits audit events: IngestStarted, IngestCompleted
// Throws std::runtime_error if ingestion fails.
[[nodiscard]] IngestResumePipelineResponse run_ingest_resume_pipeline(
    const IngestResumePipelineRequest& req, ingest::IResumeIngestor& ingestor,
    ingest::IResumeStore& resume_store, core::Services& services, core::IIdGenerator& id_gen,
    core::IClock& clock);

// ────────────────────────────────────────────────────────────────
// Index Build Pipeline
// ────────────────────────────────────────────────────────────────

struct IndexBuildPipelineRequest {
  std::string scope{
      "all"};  // "atoms"|"resumes"|"opps"|"all"  // NOLINT(readability-identifier-naming)
  std::optional<std::string> trace_id;  // NOLINT(readability-identifier-naming)
};

struct IndexBuildPipelineResponse {
  std::string run_id;       // NOLINT(readability-identifier-naming)
  size_t indexed_count{0};  // NOLINT(readability-identifier-naming)
  size_t skipped_count{0};  // NOLINT(readability-identifier-naming)
  size_t stale_count{0};    // NOLINT(readability-identifier-naming)
  std::string trace_id;     // NOLINT(readability-identifier-naming)
};

// Build or rebuild the embedding vector index for the given scope.
// provider_id identifies the embedding backend (e.g. "deterministic-stub").
// Emits audit events: IndexBuildStarted, IndexBuildCompleted
[[nodiscard]] IndexBuildPipelineResponse run_index_build_pipeline(
    const IndexBuildPipelineRequest& req, ingest::IResumeStore& resume_store,
    indexing::IIndexRunStore& index_run_store, core::Services& services,
    const std::string& provider_id, core::IIdGenerator& id_gen, core::IClock& clock);

}  // namespace ccmcp::app
