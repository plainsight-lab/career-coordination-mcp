#include "ccmcp/core/clock.h"
#include "ccmcp/core/id_generator.h"
#include "ccmcp/core/ids.h"
#include "ccmcp/core/services.h"
#include "ccmcp/domain/experience_atom.h"
#include "ccmcp/domain/opportunity.h"
#include "ccmcp/domain/requirement.h"
#include "ccmcp/embedding/embedding_provider.h"
#include "ccmcp/ingest/resume_ingestor.h"
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
#include "ccmcp/storage/sqlite/sqlite_resume_store.h"
#include "ccmcp/vector/null_embedding_index.h"

#include <nlohmann/json.hpp>

#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

struct CliConfig {
  std::optional<std::string> db_path;
  ccmcp::matching::MatchingStrategy matching_strategy{
      ccmcp::matching::MatchingStrategy::kDeterministicLexicalV01};
  std::string vector_backend{"inmemory"};  // "inmemory" or "lancedb"
};

// Forward declarations
void run_cli_logic(ccmcp::core::Services& services, ccmcp::core::DeterministicIdGenerator& id_gen,
                   ccmcp::core::FixedClock& clock, ccmcp::matching::MatchingStrategy strategy);
int run_ingest_resume(int argc, char* argv[]);

// Parse command line arguments
CliConfig parse_cli_args(int argc, char* argv[]) {
  CliConfig config;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];

    if (arg == "--db" && i + 1 < argc) {
      config.db_path = argv[++i];
    } else if (arg == "--matching-strategy" && i + 1 < argc) {
      std::string strategy = argv[++i];
      if (strategy == "hybrid") {
        config.matching_strategy = ccmcp::matching::MatchingStrategy::kHybridLexicalEmbeddingV02;
      } else if (strategy == "lexical") {
        config.matching_strategy = ccmcp::matching::MatchingStrategy::kDeterministicLexicalV01;
      } else {
        std::cerr << "Invalid --matching-strategy: " << strategy << " (valid: lexical, hybrid)\n";
      }
    } else if (arg == "--vector-backend" && i + 1 < argc) {
      std::string backend = argv[++i];
      if (backend == "inmemory" || backend == "lancedb") {
        config.vector_backend = backend;
      } else {
        std::cerr << "Invalid --vector-backend: " << backend << " (valid: inmemory, lancedb)\n";
      }
    }
  }

  return config;
}

int main(int argc, char* argv[]) {
  // Check for subcommands
  if (argc > 1) {
    std::string subcommand = argv[1];
    if (subcommand == "ingest-resume") {
      return run_ingest_resume(argc, argv);
    }
  }

  auto config = parse_cli_args(argc, argv);

  std::cout << "career-coordination-mcp v0.1\n";
  if (config.db_path.has_value()) {
    std::cout << "Using SQLite database: " << config.db_path.value() << "\n";
  }
  if (config.matching_strategy == ccmcp::matching::MatchingStrategy::kHybridLexicalEmbeddingV02) {
    std::cout << "Matching strategy: hybrid (lexical + embedding)\n";
    std::cout << "Vector backend: " << config.vector_backend << "\n";
  }

  // Check for incompatible configuration
  if (config.vector_backend == "lancedb") {
    std::cerr << "Error: LanceDB backend not yet implemented in v0.2\n";
    return 1;
  }

  // Inject deterministic generators for reproducible demo output
  ccmcp::core::DeterministicIdGenerator id_gen;
  ccmcp::core::FixedClock clock("2026-01-01T00:00:00Z");

  // Initialize repositories based on --db flag
  if (config.db_path.has_value()) {
    // SQLite persistence
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

    // Create SQLite repositories
    ccmcp::storage::sqlite::SqliteAtomRepository atom_repo(db);
    ccmcp::storage::sqlite::SqliteOpportunityRepository opportunity_repo(db);
    ccmcp::storage::sqlite::SqliteInteractionRepository interaction_repo(db);
    ccmcp::storage::sqlite::SqliteAuditLog audit_log(db);
    ccmcp::vector::NullEmbeddingIndex vector_index;
    ccmcp::embedding::NullEmbeddingProvider embedding_provider;

    ccmcp::core::Services services{atom_repo, opportunity_repo, interaction_repo,
                                   audit_log, vector_index,     embedding_provider};

    // Run application logic with SQLite services
    run_cli_logic(services, id_gen, clock, config.matching_strategy);
  } else {
    // In-memory (default)
    ccmcp::storage::InMemoryAtomRepository atom_repo;
    ccmcp::storage::InMemoryOpportunityRepository opportunity_repo;
    ccmcp::storage::InMemoryInteractionRepository interaction_repo;
    ccmcp::storage::InMemoryAuditLog audit_log;
    ccmcp::vector::NullEmbeddingIndex vector_index;
    ccmcp::embedding::NullEmbeddingProvider embedding_provider;

    ccmcp::core::Services services{atom_repo, opportunity_repo, interaction_repo,
                                   audit_log, vector_index,     embedding_provider};

    // Run application logic with in-memory services
    run_cli_logic(services, id_gen, clock, config.matching_strategy);
  }

  return 0;
}

// Ingest resume subcommand implementation
int run_ingest_resume(int argc, char* argv[]) {
  // Usage: ccmcp_cli ingest-resume <file-path> [--db <db-path>]
  if (argc < 3) {
    std::cerr << "Usage: ccmcp_cli ingest-resume <file-path> [--db <db-path>]\n";
    return 1;
  }

  std::string file_path = argv[2];
  std::optional<std::string> db_path;

  // Parse options
  for (int i = 3; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--db" && i + 1 < argc) {
      db_path = argv[++i];
    }
  }

  // If no --db specified, use default
  if (!db_path.has_value()) {
    db_path = "data/ccmcp.db";
    std::cout << "No --db specified, using default: " << db_path.value() << "\n";
  }

  // Open database and ensure schema v2
  auto db_result = ccmcp::storage::sqlite::SqliteDb::open(db_path.value());
  if (!db_result.has_value()) {
    std::cerr << "Failed to open database: " << db_result.error() << "\n";
    return 1;
  }

  auto db = db_result.value();
  auto schema_result = db->ensure_schema_v2();
  if (!schema_result.has_value()) {
    std::cerr << "Failed to initialize schema: " << schema_result.error() << "\n";
    return 1;
  }

  // Create ingestor and store
  auto ingestor = ccmcp::ingest::create_resume_ingestor();
  ccmcp::storage::sqlite::SqliteResumeStore resume_store(db);

  // Create ID generator and clock (deterministic for reproducibility)
  ccmcp::core::DeterministicIdGenerator id_gen;
  ccmcp::core::SystemClock clock;

  // Run ingestion
  std::cout << "Ingesting resume from: " << file_path << "\n";

  ccmcp::ingest::IngestOptions options;
  auto result = ingestor->ingest_file(file_path, options, id_gen, clock);

  if (!result.has_value()) {
    std::cerr << "Ingestion failed: " << result.error() << "\n";
    return 1;
  }

  auto ingested_resume = result.value();

  // Store in database
  resume_store.upsert(ingested_resume);

  // Print result
  std::cout << "Success!\n";
  std::cout << "  Resume ID: " << ingested_resume.resume_id.value << "\n";
  std::cout << "  Resume hash: " << ingested_resume.resume_hash << "\n";
  std::cout << "  Extraction method: " << ingested_resume.meta.extraction_method << "\n";
  std::cout << "  Ingestion version: " << ingested_resume.meta.ingestion_version << "\n";
  if (ingested_resume.meta.source_path.has_value()) {
    std::cout << "  Source path: " << ingested_resume.meta.source_path.value() << "\n";
  }
  std::cout << "  Resume content length: " << ingested_resume.resume_md.size() << " bytes\n";

  return 0;
}

// Application logic extracted to function for reuse with different storage backends
void run_cli_logic(ccmcp::core::Services& services, ccmcp::core::DeterministicIdGenerator& id_gen,
                   ccmcp::core::FixedClock& clock, ccmcp::matching::MatchingStrategy strategy) {
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
  ccmcp::matching::Matcher matcher(ccmcp::matching::ScoreWeights{}, strategy);
  const auto verified_atoms = services.atoms.list_verified();
  const auto report = matcher.evaluate(opportunity, verified_atoms, &services.embedding_provider,
                                       &services.vector_index);

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
}
