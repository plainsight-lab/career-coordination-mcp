#pragma once

#include "ccmcp/core/clock.h"
#include "ccmcp/core/id_generator.h"
#include "ccmcp/ingest/resume_ingestor.h"
#include "ccmcp/ingest/resume_store.h"

#include <string>

// execute_ingest_resume: ingest a resume from file_path, persist it via resume_store, and print
// results. Takes only interface types — no concrete storage headers may be included in this TU.
int execute_ingest_resume(const std::string& file_path, ccmcp::ingest::IResumeIngestor& ingestor,
                          ccmcp::ingest::IResumeStore& resume_store,
                          ccmcp::core::IIdGenerator& id_gen, ccmcp::core::IClock& clock);
