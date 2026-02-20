#include "decision_logic.h"

#include "ccmcp/app/app_service.h"
#include "ccmcp/domain/decision_record.h"

#include <nlohmann/json.hpp>

#include <iostream>
#include <string>
#include <vector>

int execute_get_decision(const std::string& decision_id, ccmcp::storage::IDecisionStore& store) {
  auto record = ccmcp::app::fetch_decision(decision_id, store);

  if (!record.has_value()) {
    std::cerr << "Decision not found: " << decision_id << "\n";
    return 1;
  }

  std::cout << ccmcp::domain::decision_record_to_json(record.value()).dump(2) << "\n";
  return 0;
}

int execute_list_decisions(const std::string& trace_id, ccmcp::storage::IDecisionStore& store) {
  auto records = ccmcp::app::list_decisions_by_trace(trace_id, store);

  nlohmann::json out;
  out["trace_id"] = trace_id;
  out["decisions"] = nlohmann::json::array();
  for (const auto& rec : records) {
    out["decisions"].push_back(ccmcp::domain::decision_record_to_json(rec));
  }

  std::cout << out.dump(2) << "\n";
  return 0;
}
