#include "ccmcp/app/app_service.h"
#include "ccmcp/core/clock.h"
#include "ccmcp/core/id_generator.h"
#include "ccmcp/core/services.h"
#include "ccmcp/embedding/embedding_provider.h"
#include "ccmcp/ingest/resume_ingestor.h"
#include "ccmcp/interaction/redis_config.h"
#include "ccmcp/interaction/redis_interaction_coordinator.h"
#include "ccmcp/storage/audit_log.h"
#include "ccmcp/storage/inmemory_atom_repository.h"
#include "ccmcp/storage/inmemory_interaction_repository.h"
#include "ccmcp/storage/inmemory_opportunity_repository.h"
#include "ccmcp/storage/sqlite/sqlite_atom_repository.h"
#include "ccmcp/storage/sqlite/sqlite_audit_log.h"
#include "ccmcp/storage/sqlite/sqlite_db.h"
#include "ccmcp/storage/sqlite/sqlite_decision_store.h"
#include "ccmcp/storage/sqlite/sqlite_index_run_store.h"
#include "ccmcp/storage/sqlite/sqlite_interaction_repository.h"
#include "ccmcp/storage/sqlite/sqlite_opportunity_repository.h"
#include "ccmcp/storage/sqlite/sqlite_resume_store.h"
#include "ccmcp/vector/inmemory_embedding_index.h"
#include "ccmcp/vector/sqlite_embedding_index.h"
#include "ccmcp/vector/vector_backend.h"

#include <nlohmann/json.hpp>

#include "config.h"
#include "server_context.h"
#include "server_loop.h"
#include "startup_guard.h"
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>

using namespace ccmcp;

// ────────────────────────────────────────────────────────────────
// Main
// ────────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
  auto config = mcp::parse_args(argc, argv);

  // Validate config before emitting any startup output so no partial messages appear on error.
  const std::string config_error = mcp::validate_mcp_server_config(config);
  if (!config_error.empty()) {
    std::cerr << config_error << "\n";
    return 1;
  }

  // ── Startup diagnostic block ──────────────────────────────────────────────
  // Every subsystem announces its operational mode. Ephemeral fallbacks are
  // logged as explicit WARNINGs — not quiet notices — because data loss on a
  // production server must be impossible to miss in operator logs.
  std::cerr << "career-coordination-mcp MCP Server v0.4\n";

  if (config.db_path.has_value()) {
    std::cerr << "Storage:     SQLite -- " << config.db_path.value() << "\n";
  } else {
    std::cerr << "WARNING: No --db path specified. Running with EPHEMERAL in-memory storage.\n"
                 "         All career data (atoms, opportunities, interactions, audit log)\n"
                 "         will be LOST on process exit. Pass --db <path> to enable persistence.\n";
  }

  // Redis is required — config was validated above; uri is guaranteed present and valid.
  {
    const auto redis_cfg = interaction::parse_redis_uri(config.redis_uri.value());
    std::cerr << "Coordinator: Redis (required) -- "
              << interaction::redis_config_to_log_string(redis_cfg.value()) << "\n";
  }

  switch (config.vector_backend) {
    case vector::VectorBackend::kSqlite:
      std::cerr << "Vector:      SQLite -- " << config.vector_db_path.value() << "/vectors.db\n";
      break;
    case vector::VectorBackend::kInMemory:
      std::cerr << "WARNING: No --vector-backend sqlite specified. Running with EPHEMERAL "
                   "in-memory vector index.\n"
                   "         Embedding index will be LOST on process exit. Hybrid matching will "
                   "require\n"
                   "         re-embedding on restart. Pass --vector-backend sqlite "
                   "--vector-db-path <dir>.\n";
      break;
    case vector::VectorBackend::kLanceDb:
      break;  // unreachable — validated and rejected above
  }

  std::cerr << "Listening on stdio for JSON-RPC requests...\n";
  // ─────────────────────────────────────────────────────────────────────────

  // Create resume ingestor — process-lifetime, shared across all handlers.
  auto ingestor_owner = ingest::create_resume_ingestor();
  ingest::IResumeIngestor& ingestor = *ingestor_owner;

  // Construct the vector index. The vector_db_path was validated above when kSqlite is selected.
  std::unique_ptr<vector::IEmbeddingIndex> vector_index_owner;
  switch (config.vector_backend) {
    case vector::VectorBackend::kSqlite: {
      const std::string& dir = config.vector_db_path.value();
      std::filesystem::create_directories(dir);
      const std::string db_file = dir + "/vectors.db";
      try {
        vector_index_owner = std::make_unique<vector::SqliteEmbeddingIndex>(db_file);
      } catch (const std::exception& e) {
        std::cerr << "Error: failed to open vector index: " << e.what() << "\n";
        return 1;
      }
      break;
    }
    case vector::VectorBackend::kInMemory:
      vector_index_owner = std::make_unique<vector::InMemoryEmbeddingIndex>();
      break;
    case vector::VectorBackend::kLanceDb:
      break;  // unreachable — validated and rejected above
  }
  vector::IEmbeddingIndex& vector_index = *vector_index_owner;

  // Inject deterministic generators for reproducible behavior
  core::DeterministicIdGenerator id_gen;
  core::FixedClock clock("2026-01-01T00:00:00Z");

  // Initialize repositories based on --db flag.
  // Redis coordinator is always used — validated at startup; uri is guaranteed present.
  if (config.db_path.has_value()) {
    // SQLite persistence path
    auto db_result = storage::sqlite::SqliteDb::open(config.db_path.value());
    if (!db_result.has_value()) {
      std::cerr << "Failed to open database: " << db_result.error() << "\n";
      return 1;
    }

    auto db = db_result.value();
    // ensure_schema_v6 chains v1→v5; all schema migrations are idempotent.
    auto schema_result = db->ensure_schema_v6();
    if (!schema_result.has_value()) {
      std::cerr << "Failed to initialize schema: " << schema_result.error() << "\n";
      return 1;
    }

    storage::sqlite::SqliteAtomRepository atom_repo(db);
    storage::sqlite::SqliteOpportunityRepository opportunity_repo(db);
    storage::sqlite::SqliteInteractionRepository interaction_repo(db);
    storage::sqlite::SqliteAuditLog audit_log(db);
    storage::sqlite::SqliteResumeStore resume_store(db);
    storage::sqlite::SqliteIndexRunStore index_run_store(db);
    storage::sqlite::SqliteDecisionStore decision_store(db);

    embedding::DeterministicStubEmbeddingProvider embedding_provider;

    core::Services services{atom_repo, opportunity_repo, interaction_repo,
                            audit_log, vector_index,     embedding_provider};

    try {
      interaction::RedisInteractionCoordinator coordinator(config.redis_uri.value());
      mcp::ServerContext ctx{services,       coordinator, ingestor, resume_store, index_run_store,
                             decision_store, id_gen,      clock,    config};
      mcp::run_server_loop(ctx);
    } catch (const std::exception& e) {
      std::cerr << "Failed to connect to Redis: " << e.what() << "\n";
      return 1;
    }

  } else {
    // Ephemeral path — no --db provided.
    //
    // There is no InMemoryResumeStore or InMemoryIndexRunStore. SqliteResumeStore and
    // SqliteIndexRunStore are the only implementations of those interfaces. A dedicated
    // in-memory SQLite database provides the same interface contract without disk I/O.
    auto mem_db_result = storage::sqlite::SqliteDb::open(":memory:");
    if (!mem_db_result.has_value()) {
      std::cerr << "Failed to open in-memory database: " << mem_db_result.error() << "\n";
      return 1;
    }

    auto mem_db = mem_db_result.value();
    auto mem_schema_result = mem_db->ensure_schema_v6();
    if (!mem_schema_result.has_value()) {
      std::cerr << "Failed to initialize in-memory schema: " << mem_schema_result.error() << "\n";
      return 1;
    }

    storage::InMemoryAtomRepository atom_repo;
    storage::InMemoryOpportunityRepository opportunity_repo;
    storage::InMemoryInteractionRepository interaction_repo;
    storage::InMemoryAuditLog audit_log;
    storage::sqlite::SqliteResumeStore resume_store(mem_db);
    storage::sqlite::SqliteIndexRunStore index_run_store(mem_db);
    storage::sqlite::SqliteDecisionStore decision_store(mem_db);

    embedding::DeterministicStubEmbeddingProvider embedding_provider;

    core::Services services{atom_repo, opportunity_repo, interaction_repo,
                            audit_log, vector_index,     embedding_provider};

    try {
      interaction::RedisInteractionCoordinator coordinator(config.redis_uri.value());
      mcp::ServerContext ctx{services,       coordinator, ingestor, resume_store, index_run_store,
                             decision_store, id_gen,      clock,    config};
      run_server_loop(ctx);
    } catch (const std::exception& e) {
      std::cerr << "Failed to connect to Redis: " << e.what() << "\n";
      return 1;
    }
  }

  return 0;
}
