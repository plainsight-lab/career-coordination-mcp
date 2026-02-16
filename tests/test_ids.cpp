#include "ccmcp/core/id_generator.h"
#include "ccmcp/core/ids.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("ID generators produce non-empty values", "[ids]") {
  SECTION("SystemIdGenerator produces non-empty IDs") {
    ccmcp::core::SystemIdGenerator gen;
    const auto atom_id = ccmcp::core::new_atom_id(gen);
    const auto trace_id = ccmcp::core::new_trace_id(gen);

    REQUIRE_FALSE(atom_id.value.empty());
    REQUIRE_FALSE(trace_id.value.empty());
  }

  SECTION("DeterministicIdGenerator produces non-empty IDs") {
    ccmcp::core::DeterministicIdGenerator gen;
    const auto atom_id = ccmcp::core::new_atom_id(gen);
    const auto trace_id = ccmcp::core::new_trace_id(gen);

    REQUIRE_FALSE(atom_id.value.empty());
    REQUIRE_FALSE(trace_id.value.empty());
  }
}
