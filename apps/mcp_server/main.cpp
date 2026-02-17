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

#include <nlohmann/json.hpp>

#include "mcp_protocol.h"
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

using json = nlohmann::json;
using namespace ccmcp;

// ────────────────────────────────────────────────────────────────
// Configuration
// ────────────────────────────────────────────────────────────────

struct McpServerConfig {
  std::optional<std::string> db_path;         // NOLINT(readability-identifier-naming)
  std::optional<std::string> redis_uri;       // NOLINT(readability-identifier-naming)
  std::string vector_backend{"inmemory"};     // NOLINT(readability-identifier-naming)
  matching::MatchingStrategy default_strategy{// NOLINT(readability-identifier-naming)
                                              matching::MatchingStrategy::kDeterministicLexicalV01};
};

McpServerConfig parse_args(int argc, char* argv[]) {
  McpServerConfig config;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];

    if (arg == "--db" && i + 1 < argc) {
      config.db_path = argv[++i];
    } else if (arg == "--redis" && i + 1 < argc) {
      config.redis_uri = argv[++i];
    } else if (arg == "--vector-backend" && i + 1 < argc) {
      std::string backend = argv[++i];
      if (backend == "inmemory" || backend == "lancedb") {
        config.vector_backend = backend;
      } else {
        std::cerr << "Invalid --vector-backend: " << backend << " (valid: inmemory, lancedb)\n";
      }
    } else if (arg == "--matching-strategy" && i + 1 < argc) {
      std::string strategy = argv[++i];
      if (strategy == "hybrid") {
        config.default_strategy = matching::MatchingStrategy::kHybridLexicalEmbeddingV02;
      } else if (strategy == "lexical") {
        config.default_strategy = matching::MatchingStrategy::kDeterministicLexicalV01;
      } else {
        std::cerr << "Invalid --matching-strategy: " << strategy << " (valid: lexical, hybrid)\n";
      }
    }
  }

  return config;
}

// ────────────────────────────────────────────────────────────────
// Server Context
// ────────────────────────────────────────────────────────────────

struct ServerContext {
  core::Services& services;                           // NOLINT(readability-identifier-naming)
  interaction::IInteractionCoordinator& coordinator;  // NOLINT(readability-identifier-naming)
  core::IIdGenerator& id_gen;                         // NOLINT(readability-identifier-naming)
  core::IClock& clock;                                // NOLINT(readability-identifier-naming)
  McpServerConfig& config;                            // NOLINT(readability-identifier-naming)
};

// Forward declaration
void run_server_loop(ServerContext& ctx);

// ────────────────────────────────────────────────────────────────
// Tool Handlers
// ────────────────────────────────────────────────────────────────

using ToolHandler = std::function<json(const json& params, ServerContext& ctx)>;

json handle_match_opportunity(const json& params, ServerContext& ctx) {
  try {
    app::MatchPipelineRequest request{
        .strategy = ctx.config.default_strategy,
    };

    // Parse strategy
    if (params.contains("strategy")) {
      std::string strategy_str = params["strategy"];
      if (strategy_str == "hybrid_lexical_embedding_v0.2") {
        request.strategy = matching::MatchingStrategy::kHybridLexicalEmbeddingV02;
      }
    }

    // Parse k_lex, k_emb
    if (params.contains("k_lex")) {
      request.k_lex = params["k_lex"];
    }
    if (params.contains("k_emb")) {
      request.k_emb = params["k_emb"];
    }

    // Parse trace_id
    if (params.contains("trace_id")) {
      request.trace_id = params["trace_id"];
    }

    // Parse opportunity (simplified: assume provided inline for now)
    if (params.contains("opportunity")) {
      // Full opportunity object parsing would go here
      // For now, throw error if not using opportunity_id
      throw std::invalid_argument("Inline opportunity not yet implemented; use opportunity_id");
    }

    if (params.contains("opportunity_id")) {
      request.opportunity_id = core::OpportunityId{params["opportunity_id"]};
    }

    // Parse atoms (simplified: default to verified atoms)
    if (params.contains("atom_ids")) {
      std::vector<core::AtomId> ids;
      for (const auto& id_str : params["atom_ids"]) {
        ids.push_back(core::AtomId{id_str});
      }
      request.atom_ids = ids;
    }

    // Run pipeline
    auto response = app::run_match_pipeline(request, ctx.services, ctx.id_gen, ctx.clock);

    // Build JSON response
    json result;
    result["trace_id"] = response.trace_id;

    result["match_report"] = {
        {"opportunity_id", response.match_report.opportunity_id.value},
        {"overall_score", response.match_report.overall_score},
        {"strategy", response.match_report.strategy},
        {"matched_atoms", json::array()},
    };
    for (const auto& atom_id : response.match_report.matched_atoms) {
      result["match_report"]["matched_atoms"].push_back(atom_id.value);
    }

    result["validation_report"] = {
        {"status",
         [&]() {
           switch (response.validation_report.status) {
             case constitution::ValidationStatus::kAccepted:
               return "accepted";
             case constitution::ValidationStatus::kRejected:
               return "rejected";
             case constitution::ValidationStatus::kBlocked:
               return "blocked";
             default:
               return "unknown";
           }
         }()},
        {"finding_count", response.validation_report.findings.size()},
    };

    return result;

  } catch (const std::exception& e) {
    json error_result;
    error_result["error"] = e.what();
    return error_result;
  }
}

json handle_validate_match_report(const json& /*params*/, ServerContext& /*ctx*/) {
  json result;
  result["error"] = "validate_match_report not yet implemented";
  return result;
}

json handle_get_audit_trace(const json& params, ServerContext& ctx) {
  try {
    std::string trace_id = params.at("trace_id");
    auto events = app::fetch_audit_trace(trace_id, ctx.services);

    json result;
    result["trace_id"] = trace_id;
    result["events"] = json::array();

    for (const auto& event : events) {
      result["events"].push_back({
          {"event_id", event.event_id},
          {"trace_id", event.trace_id},
          {"event_type", event.event_type},
          {"payload", json::parse(event.payload)},
          {"created_at", event.created_at},
      });
    }

    return result;

  } catch (const std::exception& e) {
    json error_result;
    error_result["error"] = e.what();
    return error_result;
  }
}

json handle_interaction_apply_event(const json& params, ServerContext& ctx) {
  try {
    app::InteractionTransitionRequest request{
        .interaction_id = core::InteractionId{params.at("interaction_id").get<std::string>()},
        .event =
            [&]() {
              std::string event_str = params.at("event");
              if (event_str == "Prepare") {
                return domain::InteractionEvent::kPrepare;
              }
              if (event_str == "Send") {
                return domain::InteractionEvent::kSend;
              }
              if (event_str == "ReceiveReply") {
                return domain::InteractionEvent::kReceiveReply;
              }
              if (event_str == "Close") {
                return domain::InteractionEvent::kClose;
              }
              throw std::invalid_argument("Unknown event: " + event_str);
            }(),
        .idempotency_key = params.at("idempotency_key").get<std::string>(),
    };

    if (params.contains("trace_id")) {
      request.trace_id = params["trace_id"];
    }

    auto response = app::run_interaction_transition(request, ctx.coordinator, ctx.services,
                                                    ctx.id_gen, ctx.clock);

    json result;
    result["trace_id"] = response.trace_id;
    result["result"] = {
        {"outcome",
         [&]() {
           switch (response.result.outcome) {
             case interaction::TransitionOutcome::kApplied:
               return "applied";
             case interaction::TransitionOutcome::kAlreadyApplied:
               return "already_applied";
             case interaction::TransitionOutcome::kConflict:
               return "conflict";
             case interaction::TransitionOutcome::kNotFound:
               return "not_found";
             case interaction::TransitionOutcome::kInvalidTransition:
               return "invalid_transition";
             case interaction::TransitionOutcome::kBackendError:
               return "backend_error";
             default:
               return "unknown";
           }
         }()},
        {"before_state", static_cast<int>(response.result.before_state)},
        {"after_state", static_cast<int>(response.result.after_state)},
        {"transition_index", response.result.transition_index},
    };

    return result;

  } catch (const std::exception& e) {
    json error_result;
    error_result["error"] = e.what();
    return error_result;
  }
}

// ────────────────────────────────────────────────────────────────
// Method Handlers
// ────────────────────────────────────────────────────────────────

using MethodHandler = std::function<json(const mcp::JsonRpcRequest& req, ServerContext& ctx)>;

json handle_initialize(const mcp::JsonRpcRequest& /*req*/, ServerContext& /*ctx*/) {
  return json{
      {"protocolVersion", "2024-11-05"},
      {"capabilities", {{"tools", json::object()}}},
      {"serverInfo", {{"name", "career-coordination-mcp"}, {"version", "0.2.0"}}},
  };
}

json handle_tools_list(const mcp::JsonRpcRequest& /*req*/, ServerContext& /*ctx*/) {
  json tools = json::array();

  tools.push_back({
      {"name", "match_opportunity"},
      {"description", "Run matching + validation pipeline for an opportunity"},
      {"inputSchema",
       {
           {"type", "object"},
           {"properties",
            {
                {"opportunity_id", {{"type", "string"}}},
                {"strategy", {{"type", "string"}}},
                {"k_lex", {{"type", "number"}}},
                {"k_emb", {{"type", "number"}}},
                {"trace_id", {{"type", "string"}}},
            }},
           {"required", json::array({"opportunity_id"})},
       }},
  });

  tools.push_back({
      {"name", "validate_match_report"},
      {"description", "Validate a match report (standalone)"},
      {"inputSchema",
       {
           {"type", "object"},
           {"properties", {{"match_report", {{"type", "object"}}}}},
           {"required", json::array({"match_report"})},
       }},
  });

  tools.push_back({
      {"name", "get_audit_trace"},
      {"description", "Fetch audit events by trace_id"},
      {"inputSchema",
       {
           {"type", "object"},
           {"properties", {{"trace_id", {{"type", "string"}}}}},
           {"required", json::array({"trace_id"})},
       }},
  });

  tools.push_back({
      {"name", "interaction_apply_event"},
      {"description", "Apply interaction state transition"},
      {"inputSchema",
       {
           {"type", "object"},
           {"properties",
            {
                {"interaction_id", {{"type", "string"}}},
                {"event", {{"type", "string"}}},
                {"idempotency_key", {{"type", "string"}}},
                {"trace_id", {{"type", "string"}}},
            }},
           {"required", json::array({"interaction_id", "event", "idempotency_key"})},
       }},
  });

  return json{{"tools", tools}};
}

json handle_tools_call(const mcp::JsonRpcRequest& req, ServerContext& ctx) {
  std::string tool_name = req.params.value("name", "");
  json tool_params = req.params.value("arguments", json::object());

  // Tool registry
  static std::unordered_map<std::string, ToolHandler> tool_registry = {
      {"match_opportunity", handle_match_opportunity},
      {"validate_match_report", handle_validate_match_report},
      {"get_audit_trace", handle_get_audit_trace},
      {"interaction_apply_event", handle_interaction_apply_event},
  };

  auto it = tool_registry.find(tool_name);
  if (it == tool_registry.end()) {
    json error_result;
    error_result["error"] = "Unknown tool: " + tool_name;
    return error_result;
  }

  return it->second(tool_params, ctx);
}

// ────────────────────────────────────────────────────────────────
// Main Server Loop
// ────────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
  auto config = parse_args(argc, argv);

  std::cerr << "career-coordination-mcp MCP Server v0.2\n";
  if (config.db_path.has_value()) {
    std::cerr << "Using SQLite database: " << config.db_path.value() << "\n";
  } else {
    std::cerr << "Using in-memory storage (no persistence)\n";
  }
  if (config.redis_uri.has_value()) {
    std::cerr << "Using Redis coordinator: " << config.redis_uri.value() << "\n";
  }
  std::cerr << "Listening on stdio for JSON-RPC requests...\n";

  // Check for incompatible configuration
  if (config.vector_backend == "lancedb") {
    std::cerr << "Error: LanceDB backend not yet implemented in v0.2\n";
    return 1;
  }

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

    // Vector index and embedding provider (in-memory for now)
    vector::InMemoryEmbeddingIndex vector_index;
    embedding::DeterministicStubEmbeddingProvider embedding_provider;

    core::Services services{atom_repo, opportunity_repo, interaction_repo,
                            audit_log, vector_index,     embedding_provider};

    // Coordinator based on --redis flag
    if (config.redis_uri.has_value()) {
      try {
        interaction::RedisInteractionCoordinator coordinator(config.redis_uri.value());
        ServerContext ctx{services, coordinator, id_gen, clock, config};
        run_server_loop(ctx);
      } catch (const std::exception& e) {
        std::cerr << "Failed to connect to Redis: " << e.what() << "\n";
        return 1;
      }
    } else {
      interaction::InMemoryInteractionCoordinator coordinator;
      ServerContext ctx{services, coordinator, id_gen, clock, config};
      run_server_loop(ctx);
    }

  } else {
    // In-memory (default)
    storage::InMemoryAtomRepository atom_repo;
    storage::InMemoryOpportunityRepository opportunity_repo;
    storage::InMemoryInteractionRepository interaction_repo;
    storage::InMemoryAuditLog audit_log;

    // Vector index based on config
    vector::InMemoryEmbeddingIndex vector_index;
    embedding::DeterministicStubEmbeddingProvider embedding_provider;

    core::Services services{atom_repo, opportunity_repo, interaction_repo,
                            audit_log, vector_index,     embedding_provider};

    // Coordinator (in-memory only when no persistence)
    interaction::InMemoryInteractionCoordinator coordinator;
    ServerContext ctx{services, coordinator, id_gen, clock, config};
    run_server_loop(ctx);
  }

  return 0;
}

void run_server_loop(ServerContext& ctx) {
  // Method registry
  std::unordered_map<std::string, MethodHandler> method_registry = {
      {"initialize", handle_initialize},
      {"tools/list", handle_tools_list},
      {"tools/call", handle_tools_call},
  };

  // Main loop: read JSON-RPC requests from stdin, write responses to stdout
  std::string line;
  while (std::getline(std::cin, line)) {
    if (line.empty()) {
      continue;
    }

    auto request_opt = mcp::parse_request(line);
    if (!request_opt.has_value()) {
      std::cout << mcp::make_error_response(std::nullopt, mcp::kParseError, "Invalid JSON") << "\n"
                << std::flush;
      continue;
    }

    auto request = request_opt.value();
    std::cerr << "Received: " << request.method << "\n";

    // Dispatch via method registry
    auto it = method_registry.find(request.method);
    if (it != method_registry.end()) {
      json result = it->second(request, ctx);
      std::cout << mcp::make_response(request.id, result) << "\n" << std::flush;
    } else {
      std::cout << mcp::make_error_response(request.id, mcp::kMethodNotFound,
                                            "Unknown method: " + request.method)
                << "\n"
                << std::flush;
    }
  }

  std::cerr << "MCP Server shutting down\n";
}
