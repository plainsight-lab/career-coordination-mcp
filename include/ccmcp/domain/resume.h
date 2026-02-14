#pragma once

#include "ccmcp/constitution/validation_report.h"
#include "ccmcp/core/ids.h"

#include <string>
#include <vector>

namespace ccmcp::domain {

struct Resume {
  core::ResumeId resume_id;
  core::OpportunityId opportunity_id;
  std::vector<core::AtomId> selected_atoms;
  std::string rendered_output;
  constitution::ValidationReport validation_report;
};

}  // namespace ccmcp::domain
