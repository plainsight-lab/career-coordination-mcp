#include "ccmcp/core/clock.h"
#include "ccmcp/core/id_generator.h"
#include "ccmcp/core/ids.h"
#include "ccmcp/core/services.h"
#include "ccmcp/domain/experience_atom.h"
#include "ccmcp/domain/opportunity.h"
#include "ccmcp/domain/requirement.h"
#include "ccmcp/matching/matcher.h"
#include "ccmcp/storage/audit_event.h"
#include "ccmcp/storage/audit_log.h"
#include "ccmcp/storage/inmemory_atom_repository.h"
#include "ccmcp/storage/inmemory_interaction_repository.h"
#include "ccmcp/storage/inmemory_opportunity_repository.h"
#include "ccmcp/vector/null_embedding_index.h"

#include <nlohmann/json.hpp>

#include <iostream>
#include <vector>

int main() {
  std::cout << "career-coordination-mcp v0.1\n";

  // Inject deterministic generators for reproducible demo output
  ccmcp::core::DeterministicIdGenerator id_gen;
  ccmcp::core::FixedClock clock("2026-01-01T00:00:00Z");

  // Initialize repositories and services
  ccmcp::storage::InMemoryAtomRepository atom_repo;
  ccmcp::storage::InMemoryOpportunityRepository opportunity_repo;
  ccmcp::storage::InMemoryInteractionRepository interaction_repo;
  ccmcp::storage::InMemoryAuditLog audit_log;
  ccmcp::vector::NullEmbeddingIndex vector_index;

  ccmcp::core::Services services{atom_repo, opportunity_repo, interaction_repo, audit_log,
                                 vector_index};

  auto trace_id = ccmcp::core::new_trace_id(id_gen);

  // Emit RunStarted event
  services.audit_log.append({id_gen.next("evt"),
                             trace_id.value,
                             "RunStarted",
                             R"({"cli_version":"v0.1","deterministic":true})",
                             clock.now_iso8601(),
                             {}});

  // Create and store opportunity
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

  // Create and store atoms
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

  // Perform matching using verified atoms
  ccmcp::matching::Matcher matcher;
  const auto verified_atoms = services.atoms.list_verified();
  const auto report = matcher.evaluate(opportunity, verified_atoms);

  // Emit MatchCompleted event
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

  std::cout << out.dump(2) << "\n";

  // Emit RunCompleted event
  services.audit_log.append({id_gen.next("evt"),
                             trace_id.value,
                             "RunCompleted",
                             R"({"status":"success"})",
                             clock.now_iso8601(),
                             {}});

  // Print audit trail for verification
  std::cout << "\n--- Audit Trail (trace_id=" << trace_id.value << ") ---\n";
  for (const auto& event : services.audit_log.query(trace_id.value)) {
    std::cout << event.created_at << " [" << event.event_type << "] " << event.payload << "\n";
  }

  return 0;
}
