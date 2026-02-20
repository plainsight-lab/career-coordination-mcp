#include "match_logic.h"

#include "ccmcp/app/app_service.h"
#include "ccmcp/constitution/validation_report.h"
#include "ccmcp/core/ids.h"
#include "ccmcp/domain/experience_atom.h"
#include "ccmcp/domain/opportunity.h"
#include "ccmcp/domain/requirement.h"
#include "ccmcp/matching/scorer.h"
#include "ccmcp/storage/audit_event.h"

#include <nlohmann/json.hpp>

#include <iostream>
#include <string>
#include <vector>

void run_match_demo(
    ccmcp::core::Services& services, ccmcp::core::IIdGenerator& id_gen, ccmcp::core::IClock& clock,
    ccmcp::matching::MatchingStrategy strategy,
    const std::optional<ccmcp::constitution::ConstitutionOverrideRequest>& override_req) {
  auto trace_id = ccmcp::core::new_trace_id(id_gen);

  services.audit_log.append({id_gen.next("evt"),
                             trace_id.value,
                             "RunStarted",
                             R"({"cli_version":"v0.1","deterministic":true})",
                             clock.now_iso8601(),
                             {}});

  ccmcp::domain::Opportunity opportunity{};
  opportunity.opportunity_id = ccmcp::core::new_opportunity_id(id_gen);
  opportunity.company = "ExampleCo";
  opportunity.role_title = "Principal Architect";
  opportunity.source = "manual";
  opportunity.requirements = {
      ccmcp::domain::Requirement{"C++20", {"cpp", "cpp20"}, true},
      ccmcp::domain::Requirement{"Architecture experience", {"architecture"}, true},
  };
  services.opportunities.upsert(opportunity);

  services.atoms.upsert({ccmcp::core::new_atom_id(id_gen),
                         "architecture",
                         "Architecture Leadership",
                         "Led architecture decisions",
                         {"architecture", "governance"},
                         true,
                         {}});
  services.atoms.upsert({ccmcp::core::new_atom_id(id_gen),
                         "cpp",
                         "Modern C++",
                         "Built C++20 systems",
                         {"cpp20", "systems"},
                         false,
                         {}});

  ccmcp::matching::Matcher matcher(ccmcp::matching::ScoreWeights{}, strategy);
  const auto verified_atoms = services.atoms.list_verified();
  const auto report = matcher.evaluate(opportunity, verified_atoms, &services.embedding_provider,
                                       &services.vector_index);

  services.audit_log.append({id_gen.next("evt"),
                             trace_id.value,
                             "MatchCompleted",
                             R"({"opportunity_id":")" + report.opportunity_id.value +
                                 R"(","overall_score":)" + std::to_string(report.overall_score) +
                                 "}",
                             clock.now_iso8601(),
                             {report.opportunity_id.value}});

  nlohmann::json out;
  out["opportunity_id"] = report.opportunity_id.value;
  out["strategy"] = report.strategy;
  out["scores"] = {
      {"lexical", report.breakdown.lexical},
      {"semantic", report.breakdown.semantic},
      {"bonus", report.breakdown.bonus},
      {"final", report.breakdown.final_score},
  };
  out["matched_atoms"] = nlohmann::json::array();
  for (const auto& atom_id : report.matched_atoms) {
    out["matched_atoms"].push_back(atom_id.value);
  }

  // Run constitutional validation (always; override is optional).
  // run_validation_pipeline() binds payload_hash to the artifact automatically.
  const auto validation_report =
      ccmcp::app::run_validation_pipeline(report, services, id_gen, clock, trace_id.value,
                                          override_req);

  const auto* validation_status = [&]() -> const char* {
    switch (validation_report.status) {
      case ccmcp::constitution::ValidationStatus::kAccepted:
        return "accepted";
      case ccmcp::constitution::ValidationStatus::kNeedsReview:
        return "needs_review";
      case ccmcp::constitution::ValidationStatus::kRejected:
        return "rejected";
      case ccmcp::constitution::ValidationStatus::kBlocked:
        return "blocked";
      case ccmcp::constitution::ValidationStatus::kOverridden:
        return "overridden";
    }
    return "unknown";
  }();
  out["validation_status"] = validation_status;

  std::cout << out.dump(2) << "\n";

  services.audit_log.append({id_gen.next("evt"),
                             trace_id.value,
                             "RunCompleted",
                             R"({"status":"success"})",
                             clock.now_iso8601(),
                             {}});

  std::cout << "\n--- Audit Trail (trace_id=" << trace_id.value << ") ---\n";
  for (const auto& event : services.audit_log.query(trace_id.value)) {
    std::cout << event.created_at << " [" << event.event_type << "] " << event.payload << "\n";
  }
}
