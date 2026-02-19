#include "match.h"

#include "ccmcp/app/app_service.h"
#include "ccmcp/constitution/override_request.h"
#include "ccmcp/constitution/validation_report.h"
#include "ccmcp/core/clock.h"
#include "ccmcp/core/id_generator.h"
#include "ccmcp/core/ids.h"
#include "ccmcp/core/services.h"
#include "ccmcp/domain/experience_atom.h"
#include "ccmcp/domain/opportunity.h"
#include "ccmcp/domain/requirement.h"
#include "ccmcp/embedding/embedding_provider.h"
#include "ccmcp/matching/matcher.h"
#include "ccmcp/storage/audit_event.h"
#include "ccmcp/storage/audit_log.h"
#include "ccmcp/storage/inmemory_atom_repository.h"
#include "ccmcp/storage/inmemory_interaction_repository.h"
#include "ccmcp/storage/inmemory_opportunity_repository.h"
#include "ccmcp/storage/sqlite/sqlite_atom_repository.h"
#include "ccmcp/storage/sqlite/sqlite_audit_log.h"
#include "ccmcp/storage/sqlite/sqlite_db.h"
#include "ccmcp/storage/sqlite/sqlite_interaction_repository.h"
#include "ccmcp/storage/sqlite/sqlite_opportunity_repository.h"
#include "ccmcp/vector/inmemory_embedding_index.h"
#include "ccmcp/vector/null_embedding_index.h"
#include "ccmcp/vector/sqlite_embedding_index.h"

#include <nlohmann/json.hpp>

#include "shared/arg_parser.h"
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace {

struct MatchCliConfig {
  std::optional<std::string> db_path;
  ccmcp::matching::MatchingStrategy matching_strategy{
      ccmcp::matching::MatchingStrategy::kDeterministicLexicalV01};
  std::string vector_backend{"inmemory"};
  std::optional<std::string> vector_db_path;
  // Override rail — all three flags are required together (fail-fast if partial).
  std::optional<std::string> override_rule_id;
  std::optional<std::string> override_operator_id;
  std::optional<std::string> override_reason;
};

// Hardcoded demo scenario: ExampleCo opportunity matched against two atoms.
// This is an explicit test fixture — not production matching logic.
// If override is provided, constitutional validation is run with the override applied;
// the match command will output the validation status alongside match scores.
void run_match_demo(
    ccmcp::core::Services& services, ccmcp::core::DeterministicIdGenerator& id_gen,
    ccmcp::core::FixedClock& clock, ccmcp::matching::MatchingStrategy strategy,
    const std::optional<ccmcp::constitution::ConstitutionOverrideRequest>& override) {
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
                                          override);

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

}  // namespace

int cmd_match(int argc, char* argv[]) {  // NOLINT(modernize-avoid-c-arrays)
  const std::vector<ccmcp::apps::Option<MatchCliConfig>> options = {
      {"--db", true, "Path to SQLite database file",
       [](MatchCliConfig& c, const std::string& v) {
         c.db_path = v;
         return true;
       }},
      {"--matching-strategy", true, "Matching strategy (lexical|hybrid)",
       [](MatchCliConfig& c, const std::string& v) {
         if (v == "hybrid") {
           c.matching_strategy = ccmcp::matching::MatchingStrategy::kHybridLexicalEmbeddingV02;
           return true;
         }
         if (v == "lexical") {
           c.matching_strategy = ccmcp::matching::MatchingStrategy::kDeterministicLexicalV01;
           return true;
         }
         std::cerr << "Invalid --matching-strategy: " << v << " (valid: lexical, hybrid)\n";
         return false;
       }},
      {"--vector-backend", true, "Vector backend (inmemory|sqlite)",
       [](MatchCliConfig& c, const std::string& v) {
         if (v == "inmemory" || v == "sqlite") {
           c.vector_backend = v;
           return true;
         }
         std::cerr << "Invalid --vector-backend: " << v << " (valid: inmemory, sqlite)\n";
         return false;
       }},
      {"--vector-db-path", true, "Directory for SQLite-backed vector index",
       [](MatchCliConfig& c, const std::string& v) {
         c.vector_db_path = v;
         return true;
       }},
      {"--override-rule", true, "Rule ID to override (requires --operator and --reason)",
       [](MatchCliConfig& c, const std::string& v) {
         c.override_rule_id = v;
         return true;
       }},
      {"--operator", true, "Operator ID authorizing the override (requires --override-rule)",
       [](MatchCliConfig& c, const std::string& v) {
         c.override_operator_id = v;
         return true;
       }},
      {"--reason", true, "Human-readable reason for the override (requires --override-rule)",
       [](MatchCliConfig& c, const std::string& v) {
         c.override_reason = v;
         return true;
       }},
  };
  auto config = ccmcp::apps::parse_options(argc, argv, options, 2);

  // Fail-fast: --override-rule, --operator, and --reason are an all-or-nothing set.
  // Providing any subset is a usage error — no implicit defaults are allowed.
  const bool has_any_override = config.override_rule_id.has_value() ||
                                config.override_operator_id.has_value() ||
                                config.override_reason.has_value();
  const bool has_all_override = config.override_rule_id.has_value() &&
                                config.override_operator_id.has_value() &&
                                config.override_reason.has_value();
  if (has_any_override && !has_all_override) {
    std::cerr << "Error: --override-rule requires both --operator and --reason\n";
    return 1;
  }

  // Build the override request (payload_hash is bound by run_validation_pipeline()).
  std::optional<ccmcp::constitution::ConstitutionOverrideRequest> override_req;
  if (has_all_override) {
    ccmcp::constitution::ConstitutionOverrideRequest req;
    req.rule_id = *config.override_rule_id;
    req.operator_id = *config.override_operator_id;
    req.reason = *config.override_reason;
    // payload_hash is left empty; run_validation_pipeline() computes and binds it.
    override_req = req;
  }

  std::cout << "career-coordination-mcp v0.1\n";
  if (config.db_path.has_value()) {
    std::cout << "Using SQLite database: " << config.db_path.value() << "\n";
  }
  if (config.matching_strategy == ccmcp::matching::MatchingStrategy::kHybridLexicalEmbeddingV02) {
    std::cout << "Matching strategy: hybrid (lexical + embedding)\n";
    std::cout << "Vector backend: " << config.vector_backend << "\n";
  }
  if (override_req.has_value()) {
    std::cout << "Constitutional override: rule=" << override_req->rule_id
              << " operator=" << override_req->operator_id << "\n";
  }

  if (config.vector_backend == "sqlite" && !config.vector_db_path.has_value()) {
    std::cerr << "Error: --vector-db-path <dir> is required when --vector-backend sqlite\n";
    return 1;
  }

  std::unique_ptr<ccmcp::vector::IEmbeddingIndex> vector_index_owner;
  if (config.vector_backend == "sqlite") {
    const std::string& dir = config.vector_db_path.value();
    std::filesystem::create_directories(dir);
    const std::string db_file = dir + "/vectors.db";
    try {
      vector_index_owner = std::make_unique<ccmcp::vector::SqliteEmbeddingIndex>(db_file);
      std::cout << "Using SQLite-backed vector index: " << db_file << "\n";
    } catch (const std::exception& e) {
      std::cerr << "Error: failed to open vector index: " << e.what() << "\n";
      return 1;
    }
  } else {
    vector_index_owner = std::make_unique<ccmcp::vector::NullEmbeddingIndex>();
  }
  ccmcp::vector::IEmbeddingIndex& vector_index = *vector_index_owner;

  ccmcp::core::DeterministicIdGenerator id_gen;
  ccmcp::core::FixedClock clock("2026-01-01T00:00:00Z");

  if (config.db_path.has_value()) {
    auto db_result = ccmcp::storage::sqlite::SqliteDb::open(config.db_path.value());
    if (!db_result.has_value()) {
      std::cerr << "Failed to open database: " << db_result.error() << "\n";
      return 1;
    }

    auto db = db_result.value();
    auto schema_result = db->ensure_schema_v1();
    if (!schema_result.has_value()) {
      std::cerr << "Failed to initialize schema: " << schema_result.error() << "\n";
      return 1;
    }

    ccmcp::storage::sqlite::SqliteAtomRepository atom_repo(db);
    ccmcp::storage::sqlite::SqliteOpportunityRepository opportunity_repo(db);
    ccmcp::storage::sqlite::SqliteInteractionRepository interaction_repo(db);
    ccmcp::storage::sqlite::SqliteAuditLog audit_log(db);
    ccmcp::embedding::NullEmbeddingProvider embedding_provider;

    ccmcp::core::Services services{atom_repo, opportunity_repo, interaction_repo,
                                   audit_log, vector_index,     embedding_provider};
    run_match_demo(services, id_gen, clock, config.matching_strategy, override_req);
  } else {
    ccmcp::storage::InMemoryAtomRepository atom_repo;
    ccmcp::storage::InMemoryOpportunityRepository opportunity_repo;
    ccmcp::storage::InMemoryInteractionRepository interaction_repo;
    ccmcp::storage::InMemoryAuditLog audit_log;
    ccmcp::embedding::NullEmbeddingProvider embedding_provider;

    ccmcp::core::Services services{atom_repo, opportunity_repo, interaction_repo,
                                   audit_log, vector_index,     embedding_provider};
    run_match_demo(services, id_gen, clock, config.matching_strategy, override_req);
  }

  return 0;
}
