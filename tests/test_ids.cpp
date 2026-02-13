#include <catch2/catch_test_macros.hpp>

#include "ccmcp/core/ids.h"

TEST_CASE("ID generators produce non-empty values", "[ids]") {
  const auto atom_id = ccmcp::core::new_atom_id();
  const auto trace_id = ccmcp::core::new_trace_id();

  REQUIRE_FALSE(atom_id.value.empty());
  REQUIRE_FALSE(trace_id.value.empty());
}
