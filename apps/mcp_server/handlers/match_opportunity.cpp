#include "match_opportunity.h"

#include "ccmcp/app/app_service.h"
#include "ccmcp/constitution/validation_report.h"
#include "ccmcp/core/ids.h"
#include "ccmcp/matching/matcher.h"

#include <stdexcept>
#include <string>
#include <vector>

namespace ccmcp::mcp::handlers {

using json = nlohmann::json;

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

    // Parse optional resume_id — propagated to audit trail for traceability only
    if (params.contains("resume_id") && params["resume_id"].is_string()) {
      request.resume_id = core::ResumeId{params["resume_id"].get<std::string>()};
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

    // Persist decision record (non-fatal: record the "why" but do not block the response)
    const std::string decision_id = app::record_match_decision(response, ctx.decision_store,
                                                               ctx.services, ctx.id_gen, ctx.clock);

    // Build JSON response
    json result;
    result["trace_id"] = response.trace_id;
    result["decision_id"] = decision_id;

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

}  // namespace ccmcp::mcp::handlers
