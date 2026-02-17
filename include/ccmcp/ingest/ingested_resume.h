#pragma once

#include "ccmcp/core/ids.h"
#include "ccmcp/ingest/resume_meta.h"

#include <string>

namespace ccmcp::ingest {

/// Canonical ingested resume representation
struct IngestedResume {
  core::ResumeId resume_id;               // Unique resume identifier
  std::string resume_md;                  // Canonical markdown content
  std::string resume_hash;                // SHA256 of resume_md (after hygiene)
  ResumeMeta meta;                        // Ingestion metadata
  std::optional<std::string> created_at;  // Timestamp when stored (optional)
};

}  // namespace ccmcp::ingest
