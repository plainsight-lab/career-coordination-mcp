#pragma once

#include "ccmcp/core/result.h"
#include "ccmcp/ingest/ingested_resume.h"

#include <string>

namespace ccmcp::ingest {

/// Result of resume ingestion operation
using IngestResult = core::Result<IngestedResume, std::string>;

}  // namespace ccmcp::ingest
