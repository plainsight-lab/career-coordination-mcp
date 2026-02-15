#include "ccmcp/core/ids.h"
#include "ccmcp/domain/experience_atom.h"
#include "ccmcp/domain/opportunity.h"
#include "ccmcp/domain/requirement.h"
#include "ccmcp/matching/matcher.h"
#include "ccmcp/storage/audit_event.h"
#include "ccmcp/storage/audit_log.h"

#include <nlohmann/json.hpp>

#include <iostream>
#include <vector>

int main() {
  // Enable deterministic IDs for reproducible demo output
  ccmcp::core::deterministic_ids() = true;

  std::cout << "career-coordination-mcp v0.1\n";

  // Initialize audit log
  ccmcp::storage::InMemoryAuditLog audit_log;
  auto trace_id = ccmcp::core::new_trace_id();

  // Emit RunStarted event
  audit_log.append({
      ccmcp::core::make_id("evt"),
      trace_id.value,
      "RunStarted",
      R"({"cli_version":"v0.1","deterministic":true})",
      "2026-01-01T00:00:00Z",  // Fixed timestamp for determinism
      {}
  });

  ccmcp::domain::Opportunity opportunity{};
  opportunity.opportunity_id = ccmcp::core::new_opportunity_id();
  opportunity.company = "ExampleCo";
  opportunity.role_title = "Principal Architect";
  opportunity.source = "manual";
  opportunity.requirements = {
      ccmcp::domain::Requirement{"C++20", {"cpp", "cpp20"}, true},
      ccmcp::domain::Requirement{"Architecture experience", {"architecture"}, true},
  };

  std::vector<ccmcp::domain::ExperienceAtom> atoms;
  atoms.push_back({ccmcp::core::new_atom_id(),
                   "architecture",
                   "Architecture Leadership",
                   "Led architecture decisions",
                   {"architecture", "governance"},
                   true,
                   {}});
  atoms.push_back({ccmcp::core::new_atom_id(),
                   "cpp",
                   "Modern C++",
                   "Built C++20 systems",
                   {"cpp20", "systems"},
                   false,
                   {}});

  ccmcp::matching::Matcher matcher;
  const auto report = matcher.evaluate(opportunity, atoms);

  // Emit MatchCompleted event
  audit_log.append({
      ccmcp::core::make_id("evt"),
      trace_id.value,
      "MatchCompleted",
      R"({"opportunity_id":")" + report.opportunity_id.value + R"(","overall_score":)" +
          std::to_string(report.overall_score) + "}",
      "2026-01-01T00:00:01Z",
      {report.opportunity_id.value}
  });

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
  audit_log.append({
      ccmcp::core::make_id("evt"),
      trace_id.value,
      "RunCompleted",
      R"({"status":"success"})",
      "2026-01-01T00:00:02Z",
      {}
  });

  // Print audit trail for verification
  std::cout << "\n--- Audit Trail (trace_id=" << trace_id.value << ") ---\n";
  for (const auto& event : audit_log.query(trace_id.value)) {
    std::cout << event.created_at << " [" << event.event_type << "] " << event.payload << "\n";
  }

  return 0;
}
