#include "ingest_resume.h"

#include "ccmcp/app/app_service.h"

#include <stdexcept>
#include <string>

namespace ccmcp::mcp::handlers {

using json = nlohmann::json;

json handle_ingest_resume(const json& params, ServerContext& ctx) {
  try {
    if (!params.contains("input_path") || !params["input_path"].is_string()) {
      throw std::invalid_argument("input_path (string) is required");
    }

    app::IngestResumePipelineRequest request;
    request.input_path = params["input_path"].get<std::string>();
    request.persist = params.value("persist", true);
    if (params.contains("trace_id") && params["trace_id"].is_string()) {
      request.trace_id = params["trace_id"].get<std::string>();
    }

    const auto response = app::run_ingest_resume_pipeline(request, ctx.ingestor, ctx.resume_store,
                                                          ctx.services, ctx.id_gen, ctx.clock);

    return json{
        {"resume_id", response.resume_id},
        {"resume_hash", response.resume_hash},
        {"source_hash", response.source_hash},
        {"trace_id", response.trace_id},
    };

  } catch (const std::exception& e) {
    return json{{"error", e.what()}};
  }
}

}  // namespace ccmcp::mcp::handlers
