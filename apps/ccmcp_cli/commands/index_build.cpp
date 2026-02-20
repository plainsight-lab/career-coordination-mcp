#include "index_build.h"

#include "ccmcp/core/clock.h"
#include "ccmcp/core/id_generator.h"
#include "ccmcp/embedding/embedding_provider.h"
#include "ccmcp/storage/sqlite/sqlite_atom_repository.h"
#include "ccmcp/storage/sqlite/sqlite_audit_log.h"
#include "ccmcp/storage/sqlite/sqlite_db.h"
#include "ccmcp/storage/sqlite/sqlite_index_run_store.h"
#include "ccmcp/storage/sqlite/sqlite_opportunity_repository.h"
#include "ccmcp/storage/sqlite/sqlite_resume_store.h"
#include "ccmcp/vector/inmemory_embedding_index.h"
#include "ccmcp/vector/sqlite_embedding_index.h"
#include "ccmcp/vector/vector_backend.h"

#include "index_build_logic.h"
#include "shared/arg_parser.h"
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace {

struct IndexBuildCliConfig {
  std::string db_path{"data/ccmcp.db"};
  ccmcp::vector::VectorBackend vector_backend{ccmcp::vector::VectorBackend::kInMemory};
  std::optional<std::string> vector_db_path;
  std::string scope{"all"};
  bool args_valid{true};
};

}  // namespace

int cmd_index_build(int argc, char* argv[]) {  // NOLINT(modernize-avoid-c-arrays)
  const std::vector<ccmcp::apps::Option<IndexBuildCliConfig>> options = {
      {"--db", true, "Path to SQLite database file",
       [](IndexBuildCliConfig& c, const std::string& v) {
         c.db_path = v;
         return true;
       }},
      {"--vector-backend", true, "Vector backend (inmemory|sqlite)",
       [](IndexBuildCliConfig& c, const std::string& v) {
         auto backend = ccmcp::vector::parse_vector_backend(v);
         if (!backend.has_value()) {
           std::cerr << "Invalid --vector-backend: " << v
                     << " (valid: inmemory, sqlite; lancedb is reserved and not yet "
                        "implemented)\n";
           c.args_valid = false;
           return false;
         }
         if (backend.value() == ccmcp::vector::VectorBackend::kLanceDb) {
           std::cerr << "Error: --vector-backend lancedb is reserved and not yet implemented.\n"
                        "       Use --vector-backend sqlite for persistent vector storage.\n";
           c.args_valid = false;
           return false;
         }
         c.vector_backend = backend.value();
         return true;
       }},
      {"--vector-db-path", true, "Directory for SQLite-backed vector index",
       [](IndexBuildCliConfig& c, const std::string& v) {
         c.vector_db_path = v;
         return true;
       }},
      {"--scope", true, "Index scope (atoms|resumes|opportunities|all)",
       [](IndexBuildCliConfig& c, const std::string& v) {
         if (v == "atoms" || v == "resumes" || v == "opportunities" || v == "all") {
           c.scope = v;
           return true;
         }
         std::cerr << "Invalid --scope: " << v << " (valid: atoms, resumes, opportunities, all)\n";
         c.args_valid = false;
         return false;
       }},
  };
  auto config = ccmcp::apps::parse_options(argc, argv, options, 2);

  if (!config.args_valid) {
    return 1;
  }
  if (config.vector_backend == ccmcp::vector::VectorBackend::kSqlite &&
      !config.vector_db_path.has_value()) {
    std::cerr << "Error: --vector-db-path <dir> is required when --vector-backend sqlite\n";
    return 1;
  }

  auto db_result = ccmcp::storage::sqlite::SqliteDb::open(config.db_path);
  if (!db_result.has_value()) {
    std::cerr << "Failed to open database: " << db_result.error() << "\n";
    return 1;
  }

  auto db = db_result.value();
  auto schema_result = db->ensure_schema_v6();
  if (!schema_result.has_value()) {
    std::cerr << "Failed to initialize schema: " << schema_result.error() << "\n";
    return 1;
  }

  ccmcp::storage::sqlite::SqliteAtomRepository atom_repo(db);
  ccmcp::storage::sqlite::SqliteOpportunityRepository opp_repo(db);
  ccmcp::storage::sqlite::SqliteResumeStore resume_store(db);
  ccmcp::storage::sqlite::SqliteIndexRunStore run_store(db);
  ccmcp::storage::sqlite::SqliteAuditLog audit_log(db);

  std::unique_ptr<ccmcp::vector::IEmbeddingIndex> vector_index_owner;
  switch (config.vector_backend) {
    case ccmcp::vector::VectorBackend::kSqlite: {
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
      break;
    }
    case ccmcp::vector::VectorBackend::kInMemory:
      vector_index_owner = std::make_unique<ccmcp::vector::InMemoryEmbeddingIndex>();
      break;
    case ccmcp::vector::VectorBackend::kLanceDb:
      break;  // unreachable — rejected during argument parsing above
  }

  ccmcp::embedding::DeterministicStubEmbeddingProvider embedding_provider(128);
  ccmcp::core::DeterministicIdGenerator id_gen;
  ccmcp::core::SystemClock clock;

  const ccmcp::indexing::IndexBuildConfig build_config{config.scope, "deterministic-stub", "", ""};

  std::cout << "Starting index-build: db=" << config.db_path << " scope=" << config.scope
            << " backend=" << ccmcp::vector::to_string(config.vector_backend) << "\n";

  return execute_index_build(atom_repo, opp_repo, resume_store, run_store, *vector_index_owner,
                             embedding_provider, audit_log, id_gen, clock, build_config);
}
