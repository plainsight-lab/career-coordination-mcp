#pragma once

#include "ccmcp/core/clock.h"
#include "ccmcp/core/id_generator.h"
#include "ccmcp/embedding/embedding_provider.h"
#include "ccmcp/indexing/index_build_pipeline.h"
#include "ccmcp/indexing/index_run_store.h"
#include "ccmcp/ingest/resume_store.h"
#include "ccmcp/storage/audit_log.h"
#include "ccmcp/storage/repositories.h"
#include "ccmcp/vector/embedding_index.h"

// execute_index_build: run the index build pipeline and print results.
// Takes only interface types — no concrete storage headers may be included in this TU.
int execute_index_build(ccmcp::storage::IAtomRepository& atom_repo,
                        ccmcp::storage::IOpportunityRepository& opp_repo,
                        ccmcp::ingest::IResumeStore& resume_store,
                        ccmcp::indexing::IIndexRunStore& run_store,
                        ccmcp::vector::IEmbeddingIndex& vector_index,
                        ccmcp::embedding::IEmbeddingProvider& embedding_provider,
                        ccmcp::storage::IAuditLog& audit_log, ccmcp::core::IIdGenerator& id_gen,
                        ccmcp::core::IClock& clock,
                        const ccmcp::indexing::IndexBuildConfig& build_config);
