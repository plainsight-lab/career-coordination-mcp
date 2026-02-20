#pragma once

#include "ccmcp/storage/decision_store.h"

#include <string>

// execute_get_decision: fetch and print a single decision record by ID.
// execute_list_decisions: list and print all decision records for a trace_id.
// Both take only interface types — no concrete storage headers may be included in this TU.
int execute_get_decision(const std::string& decision_id, ccmcp::storage::IDecisionStore& store);
int execute_list_decisions(const std::string& trace_id, ccmcp::storage::IDecisionStore& store);
