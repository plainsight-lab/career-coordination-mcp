#pragma once

#include "ccmcp/embedding/embedding_provider.h"
#include "ccmcp/storage/audit_log.h"
#include "ccmcp/storage/repositories.h"
#include "ccmcp/vector/embedding_index.h"

namespace ccmcp::core {

// Services is a composition root that bundles all system dependencies.
// It holds references (not ownership) to repositories and indexes.
// The CLI or other entry points are responsible for creating concrete instances
// and managing their lifetimes.
//
// Design rationale:
// - Reference semantics: ownership is explicit at the composition root
// - No copy/move: prevents accidental duplication or lifetime issues
// - Single struct: simple, transparent dependency injection
struct Services {
  storage::IAtomRepository& atoms;                    // NOLINT(readability-identifier-naming)
  storage::IOpportunityRepository& opportunities;     // NOLINT(readability-identifier-naming)
  storage::IInteractionRepository& interactions;      // NOLINT(readability-identifier-naming)
  storage::IAuditLog& audit_log;                      // NOLINT(readability-identifier-naming)
  vector::IEmbeddingIndex& vector_index;              // NOLINT(readability-identifier-naming)
  embedding::IEmbeddingProvider& embedding_provider;  // NOLINT(readability-identifier-naming)

  Services(storage::IAtomRepository& atoms, storage::IOpportunityRepository& opportunities,
           storage::IInteractionRepository& interactions, storage::IAuditLog& audit_log,
           vector::IEmbeddingIndex& vector_index, embedding::IEmbeddingProvider& embedding_provider)
      : atoms(atoms),
        opportunities(opportunities),
        interactions(interactions),
        audit_log(audit_log),
        vector_index(vector_index),
        embedding_provider(embedding_provider) {}

  ~Services() = default;

  // Prevent copying and moving to avoid accidental lifetime issues
  Services(const Services&) = delete;
  Services& operator=(const Services&) = delete;
  Services(Services&&) = delete;
  Services& operator=(Services&&) = delete;
};

}  // namespace ccmcp::core
