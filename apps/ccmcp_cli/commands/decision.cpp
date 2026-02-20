#include "decision.h"

#include "ccmcp/storage/sqlite/sqlite_db.h"
#include "ccmcp/storage/sqlite/sqlite_decision_store.h"

#include "decision_logic.h"
#include "shared/arg_parser.h"
#include <iostream>
#include <optional>
#include <string>
#include <vector>

namespace {

struct DecisionCliConfig {
  std::string db_path{"data/ccmcp.db"};
  std::optional<std::string> decision_id;
  std::optional<std::string> trace_id;
};

// Open DB, apply schema v5, and return the shared_ptr — or print error and return nullptr.
std::shared_ptr<ccmcp::storage::sqlite::SqliteDb> open_db(const std::string& path) {
  auto db_result = ccmcp::storage::sqlite::SqliteDb::open(path);
  if (!db_result.has_value()) {
    std::cerr << "Failed to open database: " << db_result.error() << "\n";
    return nullptr;
  }
  auto db = db_result.value();
  auto schema_result = db->ensure_schema_v5();
  if (!schema_result.has_value()) {
    std::cerr << "Failed to initialize schema: " << schema_result.error() << "\n";
    return nullptr;
  }
  return db;
}

}  // namespace

int cmd_get_decision(int argc, char* argv[]) {  // NOLINT(modernize-avoid-c-arrays)
  const std::vector<ccmcp::apps::Option<DecisionCliConfig>> options = {
      {"--db", true, "Path to SQLite database file",
       [](DecisionCliConfig& c, const std::string& v) {
         c.db_path = v;
         return true;
       }},
      {"--decision-id", true, "Decision record ID to fetch",
       [](DecisionCliConfig& c, const std::string& v) {
         c.decision_id = v;
         return true;
       }},
  };
  auto config = ccmcp::apps::parse_options(argc, argv, options, 2);

  if (!config.decision_id.has_value()) {
    std::cerr << "Error: --decision-id <id> is required\n";
    return 1;
  }

  auto db = open_db(config.db_path);
  if (!db) {
    return 1;
  }

  ccmcp::storage::sqlite::SqliteDecisionStore store(db);
  return execute_get_decision(config.decision_id.value(), store);
}

int cmd_list_decisions(int argc, char* argv[]) {  // NOLINT(modernize-avoid-c-arrays)
  const std::vector<ccmcp::apps::Option<DecisionCliConfig>> options = {
      {"--db", true, "Path to SQLite database file",
       [](DecisionCliConfig& c, const std::string& v) {
         c.db_path = v;
         return true;
       }},
      {"--trace-id", true, "Trace ID to list decisions for",
       [](DecisionCliConfig& c, const std::string& v) {
         c.trace_id = v;
         return true;
       }},
  };
  auto config = ccmcp::apps::parse_options(argc, argv, options, 2);

  if (!config.trace_id.has_value()) {
    std::cerr << "Error: --trace-id <id> is required\n";
    return 1;
  }

  auto db = open_db(config.db_path);
  if (!db) {
    return 1;
  }

  ccmcp::storage::sqlite::SqliteDecisionStore store(db);
  return execute_list_decisions(config.trace_id.value(), store);
}
