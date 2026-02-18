#include "ccmcp/app/app_service.h"

#include "ccmcp/constitution/match_report_view.h"
#include "ccmcp/constitution/rule.h"
#include "ccmcp/constitution/validation_engine.h"
#include "ccmcp/core/hashing.h"
#include "ccmcp/indexing/index_build_pipeline.h"
#include "ccmcp/ingest/ingest_result.h"

#include <stdexcept>

namespace ccmcp::app {

MatchPipelineResponse run_match_pipeline(const MatchPipelineRequest& req, core::Services& services,
                                         core::IIdGenerator& id_gen, core::IClock& clock) {
  // Generate or use provided trace_id
  const std::string trace_id = req.trace_id.value_or(core::TraceId{id_gen.next("trace")}.value);

  // Emit RunStarted event — include resume_id for traceability when provided
  const std::string resume_context =
      req.resume_id.has_value() ? R"(,"resume_id":")" + req.resume_id.value().value + "\"" : "";
  services.audit_log.append(
      {id_gen.next("evt"),
       trace_id,
       "RunStarted",
       R"({"source":"app_service","operation":"match_pipeline")" + resume_context + "}",
       clock.now_iso8601(),
       {}});

  // Resolve opportunity
  domain::Opportunity opportunity;
  if (req.opportunity.has_value()) {
    opportunity = req.opportunity.value();
  } else if (req.opportunity_id.has_value()) {
    auto opt_opportunity = services.opportunities.get(req.opportunity_id.value());
    if (!opt_opportunity.has_value()) {
      throw std::invalid_argument("Opportunity not found: " + req.opportunity_id.value().value);
    }
    opportunity = opt_opportunity.value();
  } else {
    throw std::invalid_argument("Must provide either opportunity or opportunity_id");
  }

  // Resolve atoms
  std::vector<domain::ExperienceAtom> atoms;
  if (req.atoms.has_value()) {
    atoms = req.atoms.value();
  } else if (req.atom_ids.has_value()) {
    for (const auto& atom_id : req.atom_ids.value()) {
      auto opt_atom = services.atoms.get(atom_id);
      if (!opt_atom.has_value()) {
        throw std::invalid_argument("Atom not found: " + atom_id.value);
      }
      atoms.push_back(opt_atom.value());
    }
  } else {
    // Default: use all verified atoms
    atoms = services.atoms.list_verified();
  }

  // Run matcher
  matching::Matcher matcher(matching::ScoreWeights{}, req.strategy);
  const auto match_report =
      matcher.evaluate(opportunity, atoms, &services.embedding_provider, &services.vector_index);

  // Emit MatchCompleted event
  services.audit_log.append({id_gen.next("evt"),
                             trace_id,
                             "MatchCompleted",
                             R"({"opportunity_id":")" + match_report.opportunity_id.value +
                                 R"(","overall_score":)" +
                                 std::to_string(match_report.overall_score) + "}",
                             clock.now_iso8601(),
                             {match_report.opportunity_id.value}});

  // Run validation
  auto validation_report = run_validation_pipeline(match_report, services, id_gen, clock, trace_id);

  // Emit RunCompleted event
  services.audit_log.append({id_gen.next("evt"),
                             trace_id,
                             "RunCompleted",
                             R"({"status":"success"})",
                             clock.now_iso8601(),
                             {}});

  return MatchPipelineResponse{
      .trace_id = trace_id,
      .match_report = match_report,
      .validation_report = validation_report,
  };
}

constitution::ValidationReport run_validation_pipeline(const domain::MatchReport& report,
                                                       core::Services& services,
                                                       core::IIdGenerator& id_gen,
                                                       core::IClock& clock,
                                                       const std::string& trace_id) {
  // Create envelope with typed artifact view
  auto view = std::make_shared<constitution::MatchReportView>(report);
  constitution::ArtifactEnvelope envelope;
  envelope.artifact_id = "match-report-" + report.opportunity_id.value;
  envelope.artifact = view;

  // Create validation context
  constitution::ValidationContext context;
  context.constitution_id = "default";
  context.constitution_version = "0.1.0";
  context.trace_id = trace_id;

  // Run validation
  constitution::ValidationEngine engine(constitution::make_default_constitution());
  auto validation_report = engine.validate(envelope, context);

  // Emit ValidationCompleted event
  const std::string status_str = [&]() {
    switch (validation_report.status) {
      case constitution::ValidationStatus::kAccepted:
        return "accepted";
      case constitution::ValidationStatus::kRejected:
        return "rejected";
      case constitution::ValidationStatus::kBlocked:
        return "blocked";
      default:
        return "unknown";
    }
  }();

  services.audit_log.append({id_gen.next("evt"),
                             trace_id,
                             "ValidationCompleted",
                             R"({"status":")" + status_str + R"(","finding_count":)" +
                                 std::to_string(validation_report.findings.size()) + "}",
                             clock.now_iso8601(),
                             {report.opportunity_id.value}});

  return validation_report;
}

InteractionTransitionResponse run_interaction_transition(
    const InteractionTransitionRequest& req, interaction::IInteractionCoordinator& coordinator,
    core::Services& services, core::IIdGenerator& id_gen, core::IClock& clock) {
  // Generate or use provided trace_id
  const std::string trace_id = req.trace_id.value_or(core::TraceId{id_gen.next("trace")}.value);

  // Emit TransitionAttempted event
  const std::string event_name = [&]() {
    switch (req.event) {
      case domain::InteractionEvent::kPrepare:
        return "Prepare";
      case domain::InteractionEvent::kSend:
        return "Send";
      case domain::InteractionEvent::kReceiveReply:
        return "ReceiveReply";
      case domain::InteractionEvent::kClose:
        return "Close";
      default:
        return "Unknown";
    }
  }();

  services.audit_log.append({id_gen.next("evt"),
                             trace_id,
                             "InteractionTransitionAttempted",
                             R"({"interaction_id":")" + req.interaction_id.value +
                                 R"(","event":")" + event_name + R"(","idempotency_key":")" +
                                 req.idempotency_key + R"("})",
                             clock.now_iso8601(),
                             {req.interaction_id.value}});

  // Apply transition via coordinator
  auto result = coordinator.apply_transition(req.interaction_id, req.event, req.idempotency_key);

  // Emit completion/rejection event
  const std::string outcome_str = [&]() {
    switch (result.outcome) {
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
  }();

  const bool success = (result.outcome == interaction::TransitionOutcome::kApplied ||
                        result.outcome == interaction::TransitionOutcome::kAlreadyApplied);

  services.audit_log.append(
      {id_gen.next("evt"),
       trace_id,
       success ? "InteractionTransitionCompleted" : "InteractionTransitionRejected",
       R"({"outcome":")" + outcome_str + R"(","transition_index":)" +
           std::to_string(result.transition_index) + "}",
       clock.now_iso8601(),
       {req.interaction_id.value}});

  return InteractionTransitionResponse{
      .trace_id = trace_id,
      .result = result,
  };
}

std::vector<storage::AuditEvent> fetch_audit_trace(const std::string& trace_id,
                                                   core::Services& services) {
  return services.audit_log.query(trace_id);
}

IngestResumePipelineResponse run_ingest_resume_pipeline(const IngestResumePipelineRequest& req,
                                                        ingest::IResumeIngestor& ingestor,
                                                        ingest::IResumeStore& resume_store,
                                                        core::Services& services,
                                                        core::IIdGenerator& id_gen,
                                                        core::IClock& clock) {
  const std::string trace_id = req.trace_id.value_or(core::TraceId{id_gen.next("trace")}.value);

  services.audit_log.append({id_gen.next("evt"),
                             trace_id,
                             "IngestStarted",
                             R"({"source":"app_service","operation":"ingest_resume","persist":)" +
                                 std::string(req.persist ? "true" : "false") + "}",
                             clock.now_iso8601(),
                             {}});

  ingest::IngestOptions options;
  auto result = ingestor.ingest_file(req.input_path, options, id_gen, clock);
  if (!result.has_value()) {
    throw std::runtime_error("Ingestion failed: " + result.error());
  }

  const auto& resume = result.value();
  const std::string source_hash = core::stable_hash64_hex(resume.resume_md);

  if (req.persist) {
    resume_store.upsert(resume);
  }

  services.audit_log.append({id_gen.next("evt"),
                             trace_id,
                             "IngestCompleted",
                             R"({"resume_id":")" + resume.resume_id.value + R"(","resume_hash":")" +
                                 resume.resume_hash + R"(","source_hash":")" + source_hash +
                                 R"(","persisted":)" + std::string(req.persist ? "true" : "false") +
                                 "}",
                             clock.now_iso8601(),
                             {resume.resume_id.value}});

  return IngestResumePipelineResponse{
      .resume_id = resume.resume_id.value,
      .resume_hash = resume.resume_hash,
      .source_hash = source_hash,
      .trace_id = trace_id,
  };
}

IndexBuildPipelineResponse run_index_build_pipeline(
    const IndexBuildPipelineRequest& req, ingest::IResumeStore& resume_store,
    indexing::IIndexRunStore& index_run_store, core::Services& services,
    const std::string& provider_id, core::IIdGenerator& id_gen, core::IClock& clock) {
  const std::string trace_id = req.trace_id.value_or(core::TraceId{id_gen.next("trace")}.value);

  services.audit_log.append(
      {id_gen.next("evt"),
       trace_id,
       "IndexBuildStarted",
       R"({"source":"app_service","operation":"index_build","scope":")" + req.scope + "\"}",
       clock.now_iso8601(),
       {}});

  const indexing::IndexBuildConfig build_config{req.scope, provider_id, "", ""};
  const auto result =
      indexing::run_index_build(services.atoms, resume_store, services.opportunities,
                                index_run_store, services.vector_index, services.embedding_provider,
                                services.audit_log, id_gen, clock, build_config);

  services.audit_log.append({id_gen.next("evt"),
                             trace_id,
                             "IndexBuildCompleted",
                             R"({"run_id":")" + result.run_id + R"(","indexed":)" +
                                 std::to_string(result.indexed_count) + R"(,"skipped":)" +
                                 std::to_string(result.skipped_count) + R"(,"stale":)" +
                                 std::to_string(result.stale_count) + "}",
                             clock.now_iso8601(),
                             {}});

  return IndexBuildPipelineResponse{
      .run_id = result.run_id,
      .indexed_count = result.indexed_count,
      .skipped_count = result.skipped_count,
      .stale_count = result.stale_count,
      .trace_id = trace_id,
  };
}

}  // namespace ccmcp::app
