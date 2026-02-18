#pragma once

// cmd_get_decision: fetch and print a single decision record by --decision-id
// cmd_list_decisions: fetch and print all decisions for a --trace-id
int cmd_get_decision(int argc, char* argv[]);    // NOLINT(modernize-avoid-c-arrays)
int cmd_list_decisions(int argc, char* argv[]);  // NOLINT(modernize-avoid-c-arrays)
