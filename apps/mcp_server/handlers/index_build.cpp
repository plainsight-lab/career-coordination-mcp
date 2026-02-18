#include "index_build.h"

#include "ccmcp/app/app_service.h"

#include <stdexcept>
#include <string>

namespace ccmcp::mcp::handlers {

using json = nlohmann::json;

// Valid scope values accepted by the index_build tool.
static const std::string kValidScopes = "atoms|resumes|opps|all";

json handle_index_build(const json& params, ServerContext& ctx) {
  try {
    const std::string scope = params.value("scope", "all");

    if (scope != "atoms" && scope != "resumes" && scope != "opps" && scope != "all") {
      throw std::invalid_argument("Invalid scope: \"" + scope + "\" (valid: " + kValidScopes + ")");
    }

    app::IndexBuildPipelineRequest request;
    request.scope = scope;
    if (params.contains("trace_id") && params["trace_id"].is_string()) {
      request.trace_id = params["trace_id"].get<std::string>();
    }

    const auto response =
        app::run_index_build_pipeline(request, ctx.resume_store, ctx.index_run_store, ctx.services,
                                      "deterministic-stub", ctx.id_gen, ctx.clock);

    return json{
        {"run_id", response.run_id},
        {"counts",
         {
             {"indexed", response.indexed_count},
             {"skipped", response.skipped_count},
             {"stale", response.stale_count},
         }},
        {"trace_id", response.trace_id},
    };

  } catch (const std::exception& e) {
    return json{{"error", e.what()}};
  }
}

}  // namespace ccmcp::mcp::handlers
