#include "ccmcp/app/app_service.h"
#include "ccmcp/core/clock.h"
#include "ccmcp/core/id_generator.h"
#include "ccmcp/core/services.h"
#include "ccmcp/embedding/embedding_provider.h"
#include "ccmcp/interaction/inmemory_interaction_coordinator.h"
#include "ccmcp/interaction/redis_interaction_coordinator.h"
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

#include "config.h"
#include "server_context.h"
#include "server_loop.h"
#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

using namespace ccmcp;

// ────────────────────────────────────────────────────────────────
// Main
// ────────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
  auto config = mcp::parse_args(argc, argv);

  // Validate config before emitting any startup output so no partial messages appear on error.
  if (config.vector_backend == "sqlite" && !config.vector_db_path.has_value()) {
    std::cerr << "Error: --vector-db-path <dir> is required when --vector-backend sqlite\n";
    return 1;
  }

  // ── Startup diagnostic block ──────────────────────────────────────────────
  // Every subsystem announces its operational mode. Ephemeral fallbacks are
  // logged as explicit WARNINGs — not quiet notices — because data loss on a
  // production server must be impossible to miss in operator logs.
  std::cerr << "career-coordination-mcp MCP Server v0.2\n";

  if (config.db_path.has_value()) {
    std::cerr << "Storage:     SQLite -- " << config.db_path.value() << "\n";
  } else {
    std::cerr << "WARNING: No --db path specified. Running with EPHEMERAL in-memory storage.\n"
                 "         All career data (atoms, opportunities, interactions, audit log)\n"
                 "         will be LOST on process exit. Pass --db <path> to enable persistence.\n";
  }

  if (config.redis_uri.has_value()) {
    std::cerr << "Coordinator: Redis -- " << config.redis_uri.value() << "\n";
  } else {
    std::cerr << "WARNING: No --redis URI specified. Running with EPHEMERAL in-memory interaction "
                 "coordinator.\n"
                 "         Interaction state will NOT survive process restart.\n"
                 "         Pass --redis <uri> to enable durable coordination.\n";
  }

  if (config.vector_backend == "sqlite") {
    std::cerr << "Vector:      SQLite -- " << config.vector_db_path.value() << "/vectors.db\n";
  } else {
    std::cerr
        << "WARNING: No --vector-backend sqlite specified. Running with EPHEMERAL in-memory vector "
           "index.\n"
           "         Embedding index will be LOST on process exit. Hybrid matching will require\n"
           "         re-embedding on restart. Pass --vector-backend sqlite --vector-db-path "
           "<dir>.\n";
  }

  std::cerr << "Listening on stdio for JSON-RPC requests...\n";
  // ─────────────────────────────────────────────────────────────────────────

  // Construct the vector index. The sqlite path was validated above.
  std::unique_ptr<vector::IEmbeddingIndex> vector_index_owner;
  if (config.vector_backend == "sqlite") {
    const std::string& dir = config.vector_db_path.value();
    std::filesystem::create_directories(dir);
    const std::string db_file = dir + "/vectors.db";
    try {
      vector_index_owner = std::make_unique<vector::SqliteEmbeddingIndex>(db_file);
    } catch (const std::exception& e) {
      std::cerr << "Error: failed to open vector index: " << e.what() << "\n";
      return 1;
    }
  } else {
    vector_index_owner = std::make_unique<vector::InMemoryEmbeddingIndex>();
  }
  vector::IEmbeddingIndex& vector_index = *vector_index_owner;

  // Inject deterministic generators for reproducible behavior
  core::DeterministicIdGenerator id_gen;
  core::FixedClock clock("2026-01-01T00:00:00Z");

  // Initialize repositories based on --db flag
  if (config.db_path.has_value()) {
    // SQLite persistence
    auto db_result = storage::sqlite::SqliteDb::open(config.db_path.value());
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
    storage::sqlite::SqliteAtomRepository atom_repo(db);
    storage::sqlite::SqliteOpportunityRepository opportunity_repo(db);
    storage::sqlite::SqliteInteractionRepository interaction_repo(db);
    storage::sqlite::SqliteAuditLog audit_log(db);

    embedding::DeterministicStubEmbeddingProvider embedding_provider;

    core::Services services{atom_repo, opportunity_repo, interaction_repo,
                            audit_log, vector_index,     embedding_provider};

    // Coordinator based on --redis flag
    if (config.redis_uri.has_value()) {
      try {
        interaction::RedisInteractionCoordinator coordinator(config.redis_uri.value());
        mcp::ServerContext ctx{services, coordinator, id_gen, clock, config};
        mcp::run_server_loop(ctx);
      } catch (const std::exception& e) {
        std::cerr << "Failed to connect to Redis: " << e.what() << "\n";
        return 1;
      }
    } else {
      interaction::InMemoryInteractionCoordinator coordinator;
      mcp::ServerContext ctx{services, coordinator, id_gen, clock, config};
      mcp::run_server_loop(ctx);
    }

  } else {
    // In-memory (default)
    storage::InMemoryAtomRepository atom_repo;
    storage::InMemoryOpportunityRepository opportunity_repo;
    storage::InMemoryInteractionRepository interaction_repo;
    storage::InMemoryAuditLog audit_log;

    embedding::DeterministicStubEmbeddingProvider embedding_provider;

    core::Services services{atom_repo, opportunity_repo, interaction_repo,
                            audit_log, vector_index,     embedding_provider};

    // Coordinator (in-memory only when no persistence)
    interaction::InMemoryInteractionCoordinator coordinator;
    mcp::ServerContext ctx{services, coordinator, id_gen, clock, config};
    run_server_loop(ctx);
  }

  return 0;
}
