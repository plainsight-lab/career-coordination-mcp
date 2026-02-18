#include "ccmcp/indexing/index_build_pipeline.h"

#include "ccmcp/core/hashing.h"
#include "ccmcp/storage/audit_event.h"

#include <nlohmann/json.hpp>

#include <string>

namespace ccmcp::indexing {

namespace {

// Build canonical text for a single atom: title + claim + tags (space-joined).
std::string atom_canonical_text(const domain::ExperienceAtom& atom) {
  std::string text = atom.title + " " + atom.claim;
  for (const auto& tag : atom.tags) {
    text += " " + tag;
  }
  return text;
}

// Build canonical text for an opportunity: role_title + company + requirement texts.
std::string opportunity_canonical_text(const domain::Opportunity& opp) {
  std::string text = opp.role_title + " " + opp.company;
  for (const auto& req : opp.requirements) {
    text += " " + req.text;
  }
  return text;
}

// Compute a hash of the raw float bytes of a vector.
std::string vector_hash(const vector::Vector& vec) {
  if (vec.empty()) {
    return core::stable_hash64_hex("");
  }
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
  std::string_view bytes(reinterpret_cast<const char*>(vec.data()), vec.size() * sizeof(float));
  return core::stable_hash64_hex(bytes);
}

// Emit a single audit event using run_id as the trace_id.
void emit_audit(storage::IAuditLog& audit_log, core::IIdGenerator& id_gen,
                const std::string& run_id, const std::string& event_type,
                const std::string& payload, const std::string& timestamp) {
  audit_log.append({id_gen.next("evt"), run_id, event_type, payload, timestamp, {}});
}

}  // namespace

IndexBuildResult run_index_build(storage::IAtomRepository& atoms, ingest::IResumeStore& resumes,
                                 storage::IOpportunityRepository& opps, IIndexRunStore& run_store,
                                 vector::IEmbeddingIndex& vector_index,
                                 embedding::IEmbeddingProvider& embedding_provider,
                                 storage::IAuditLog& audit_log, core::IIdGenerator& id_gen,
                                 core::IClock& clock, const IndexBuildConfig& config) {
  const std::string run_id = id_gen.next("run");
  const std::string started_at = clock.now_iso8601();

  // Create run record in kRunning state.
  IndexRun run{run_id,
               started_at,
               std::nullopt,
               config.provider_id,
               config.model_id,
               config.prompt_version,
               IndexRunStatus::kRunning,
               "{}"};
  run_store.upsert_run(run);

  // Emit IndexRunStarted.
  nlohmann::json started_payload;
  started_payload["run_id"] = run_id;
  started_payload["scope"] = config.scope;
  started_payload["provider_id"] = config.provider_id;
  emit_audit(audit_log, id_gen, run_id, "IndexRunStarted", started_payload.dump(), started_at);

  size_t indexed_count = 0;
  size_t skipped_count = 0;
  size_t stale_count = 0;

  // Process atoms.
  if (config.scope == "atoms" || config.scope == "all") {
    for (const auto& atom : atoms.list_all()) {
      const std::string artifact_id = atom.atom_id.value;
      const std::string artifact_type = "atom";
      const std::string canonical_text = atom_canonical_text(atom);
      const std::string src_hash = core::stable_hash64_hex(canonical_text);

      const auto prior_hash =
          run_store.get_last_source_hash(artifact_id, artifact_type, config.provider_id,
                                         config.model_id, config.prompt_version);

      if (prior_hash.has_value() && prior_hash.value() == src_hash) {
        ++skipped_count;
        continue;
      }

      const bool is_stale = prior_hash.has_value();

      const auto embedding = embedding_provider.embed_text(canonical_text);
      if (embedding.empty()) {
        // NullEmbeddingProvider: skip without recording an entry.
        continue;
      }

      const std::string vec_hash = vector_hash(embedding);
      nlohmann::json metadata;
      metadata["artifact_type"] = artifact_type;
      metadata["artifact_id"] = artifact_id;
      metadata["source_hash"] = src_hash;

      vector_index.upsert(artifact_id, embedding, metadata.dump());

      const std::string indexed_at = clock.now_iso8601();
      run_store.upsert_entry({run_id, artifact_type, artifact_id, src_hash, vec_hash, indexed_at});

      nlohmann::json event_payload;
      event_payload["artifact_type"] = artifact_type;
      event_payload["artifact_id"] = artifact_id;
      event_payload["source_hash"] = src_hash;
      event_payload["stale"] = is_stale;
      emit_audit(audit_log, id_gen, run_id, "IndexedArtifact", event_payload.dump(), indexed_at);

      ++indexed_count;
      if (is_stale) {
        ++stale_count;
      }
    }
  }

  // Process resumes.
  if (config.scope == "resumes" || config.scope == "all") {
    for (const auto& resume : resumes.list_all()) {
      const std::string artifact_id = resume.resume_id.value;
      const std::string artifact_type = "resume";
      const std::string& canonical_text = resume.resume_md;
      const std::string src_hash = core::stable_hash64_hex(canonical_text);

      const auto prior_hash =
          run_store.get_last_source_hash(artifact_id, artifact_type, config.provider_id,
                                         config.model_id, config.prompt_version);

      if (prior_hash.has_value() && prior_hash.value() == src_hash) {
        ++skipped_count;
        continue;
      }

      const bool is_stale = prior_hash.has_value();

      const auto embedding = embedding_provider.embed_text(canonical_text);
      if (embedding.empty()) {
        continue;
      }

      const std::string vec_hash = vector_hash(embedding);
      nlohmann::json metadata;
      metadata["artifact_type"] = artifact_type;
      metadata["artifact_id"] = artifact_id;
      metadata["source_hash"] = src_hash;

      const std::string vector_key = "resume:" + artifact_id;
      vector_index.upsert(vector_key, embedding, metadata.dump());

      const std::string indexed_at = clock.now_iso8601();
      run_store.upsert_entry({run_id, artifact_type, artifact_id, src_hash, vec_hash, indexed_at});

      nlohmann::json event_payload;
      event_payload["artifact_type"] = artifact_type;
      event_payload["artifact_id"] = artifact_id;
      event_payload["source_hash"] = src_hash;
      event_payload["stale"] = is_stale;
      emit_audit(audit_log, id_gen, run_id, "IndexedArtifact", event_payload.dump(), indexed_at);

      ++indexed_count;
      if (is_stale) {
        ++stale_count;
      }
    }
  }

  // Process opportunities.
  if (config.scope == "opportunities" || config.scope == "all") {
    for (const auto& opp : opps.list_all()) {
      const std::string artifact_id = opp.opportunity_id.value;
      const std::string artifact_type = "opportunity";
      const std::string canonical_text = opportunity_canonical_text(opp);
      const std::string src_hash = core::stable_hash64_hex(canonical_text);

      const auto prior_hash =
          run_store.get_last_source_hash(artifact_id, artifact_type, config.provider_id,
                                         config.model_id, config.prompt_version);

      if (prior_hash.has_value() && prior_hash.value() == src_hash) {
        ++skipped_count;
        continue;
      }

      const bool is_stale = prior_hash.has_value();

      const auto embedding = embedding_provider.embed_text(canonical_text);
      if (embedding.empty()) {
        continue;
      }

      const std::string vec_hash = vector_hash(embedding);
      nlohmann::json metadata;
      metadata["artifact_type"] = artifact_type;
      metadata["artifact_id"] = artifact_id;
      metadata["source_hash"] = src_hash;

      const std::string vector_key = "opp:" + artifact_id;
      vector_index.upsert(vector_key, embedding, metadata.dump());

      const std::string indexed_at = clock.now_iso8601();
      run_store.upsert_entry({run_id, artifact_type, artifact_id, src_hash, vec_hash, indexed_at});

      nlohmann::json event_payload;
      event_payload["artifact_type"] = artifact_type;
      event_payload["artifact_id"] = artifact_id;
      event_payload["source_hash"] = src_hash;
      event_payload["stale"] = is_stale;
      emit_audit(audit_log, id_gen, run_id, "IndexedArtifact", event_payload.dump(), indexed_at);

      ++indexed_count;
      if (is_stale) {
        ++stale_count;
      }
    }
  }

  // Build summary and complete the run.
  nlohmann::json summary;
  summary["indexed"] = indexed_count;
  summary["skipped"] = skipped_count;
  summary["stale"] = stale_count;
  summary["scope"] = config.scope;

  const std::string completed_at = clock.now_iso8601();
  run.status = IndexRunStatus::kCompleted;
  run.completed_at = completed_at;
  run.summary_json = summary.dump();
  run_store.upsert_run(run);

  nlohmann::json completed_payload;
  completed_payload["run_id"] = run_id;
  completed_payload["indexed"] = indexed_count;
  completed_payload["skipped"] = skipped_count;
  completed_payload["stale"] = stale_count;
  emit_audit(audit_log, id_gen, run_id, "IndexRunCompleted", completed_payload.dump(),
             completed_at);

  return {indexed_count, skipped_count, stale_count, run_id};
}

}  // namespace ccmcp::indexing
