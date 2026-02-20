#include "index_build_logic.h"

#include <iostream>

int execute_index_build(ccmcp::storage::IAtomRepository& atom_repo,
                        ccmcp::storage::IOpportunityRepository& opp_repo,
                        ccmcp::ingest::IResumeStore& resume_store,
                        ccmcp::indexing::IIndexRunStore& run_store,
                        ccmcp::vector::IEmbeddingIndex& vector_index,
                        ccmcp::embedding::IEmbeddingProvider& embedding_provider,
                        ccmcp::storage::IAuditLog& audit_log, ccmcp::core::IIdGenerator& id_gen,
                        ccmcp::core::IClock& clock,
                        const ccmcp::indexing::IndexBuildConfig& build_config) {
  const auto result =
      ccmcp::indexing::run_index_build(atom_repo, resume_store, opp_repo, run_store, vector_index,
                                       embedding_provider, audit_log, id_gen, clock, build_config);

  std::cout << "Index build complete:\n";
  std::cout << "  run_id:  " << result.run_id << "\n";
  std::cout << "  indexed: " << result.indexed_count << "\n";
  std::cout << "  skipped: " << result.skipped_count << "\n";
  std::cout << "  stale:   " << result.stale_count << "\n";

  return 0;
}
