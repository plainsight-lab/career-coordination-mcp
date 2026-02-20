#pragma once

#include "ccmcp/constitution/override_request.h"
#include "ccmcp/core/clock.h"
#include "ccmcp/core/id_generator.h"
#include "ccmcp/core/services.h"
#include "ccmcp/matching/matcher.h"

#include <optional>

// run_match_demo: run the hardcoded ExampleCo match demo, constitutional validation, and print
// results. Takes only interface types — no concrete storage headers may be included in this TU.
void run_match_demo(
    ccmcp::core::Services& services, ccmcp::core::IIdGenerator& id_gen, ccmcp::core::IClock& clock,
    ccmcp::matching::MatchingStrategy strategy,
    const std::optional<ccmcp::constitution::ConstitutionOverrideRequest>& override_req);
