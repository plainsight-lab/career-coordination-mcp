#include "ccmcp/app/app_service.h"

#include "ccmcp/constitution/match_report_view.h"
#include "ccmcp/constitution/rule.h"
#include "ccmcp/constitution/validation_engine.h"

#include <stdexcept>

namespace ccmcp::app {

MatchPipelineResponse run_match_pipeline(const MatchPipelineRequest& req, core::Services& services,
                                         core::IIdGenerator& id_gen, core::IClock& clock) {
  // Generate or use provided trace_id
  const std::string trace_id = req.trace_id.value_or(core::TraceId{id_gen.next("trace")}.value);

  // Emit RunStarted event
  services.audit_log.append({id_gen.next("evt"),
                             trace_id,
                             "RunStarted",
                             R"({"source":"app_service","operation":"match_pipeline"})",
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

}  // namespace ccmcp::app
