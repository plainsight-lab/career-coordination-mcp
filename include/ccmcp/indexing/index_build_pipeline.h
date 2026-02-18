#pragma once

#include "ccmcp/core/clock.h"
#include "ccmcp/core/id_generator.h"
#include "ccmcp/embedding/embedding_provider.h"
#include "ccmcp/indexing/index_run_store.h"
#include "ccmcp/ingest/resume_store.h"
#include "ccmcp/storage/audit_log.h"
#include "ccmcp/storage/repositories.h"
#include "ccmcp/vector/embedding_index.h"

#include <cstddef>
#include <string>

namespace ccmcp::indexing {

// Configuration for an index build run.
// scope controls which artifact types are indexed.
// provider_id, model_id, and prompt_version are recorded in the run for
// drift detection: a change in any of these values forces full re-indexing.
struct IndexBuildConfig {
  std::string scope;           // "atoms" | "resumes" | "opportunities" | "all"
  std::string provider_id;     // e.g. "deterministic-stub"
  std::string model_id;        // e.g. "" for stub
  std::string prompt_version;  // e.g. "" for stub
};

// Result of a completed index build run.
// indexed_count: embeddings computed and stored (new + stale artifacts)
// skipped_count: source_hash unchanged since last completed run
// stale_count: source_hash changed — artifact was re-indexed (subset of indexed_count)
// run_id: the IndexRun.run_id for this build
struct IndexBuildResult {
  size_t indexed_count{0};
  size_t skipped_count{0};
  size_t stale_count{0};
  std::string run_id;
};

// run_index_build executes a full index build for the given scope.
// For each in-scope artifact:
//   1. Computes canonical text and its source_hash.
//   2. Checks run_store for a prior source_hash (drift detection).
//   3. If hash is unchanged: skips embedding computation.
//   4. If hash changed or absent: computes embedding, upserts into vector_index,
//      writes an IndexEntry, and emits an IndexedArtifact audit event.
//   5. NullEmbeddingProvider (empty vector) suppresses indexing without error.
// Emits IndexRunStarted, IndexedArtifact, and IndexRunCompleted audit events
// using the run_id as trace_id.
IndexBuildResult run_index_build(storage::IAtomRepository& atoms, ingest::IResumeStore& resumes,
                                 storage::IOpportunityRepository& opps, IIndexRunStore& run_store,
                                 vector::IEmbeddingIndex& vector_index,
                                 embedding::IEmbeddingProvider& embedding_provider,
                                 storage::IAuditLog& audit_log, core::IIdGenerator& id_gen,
                                 core::IClock& clock, const IndexBuildConfig& config);

}  // namespace ccmcp::indexing
