add_executable(ccmcp_tests
  test_ids.cpp
  test_validation_engine.cpp
  test_matching_stub.cpp
  test_domain_schema.cpp
  test_matcher_basic_overlap.cpp
  test_matcher_determinism.cpp
  test_matcher_tie_break.cpp
  test_matcher_missing.cpp
  test_validation_schema_block.cpp
  test_validation_evidence_fail.cpp
  test_validation_warn.cpp
  test_validation_pass.cpp
  test_inmemory_atom_repository.cpp
  test_inmemory_opportunity_repository.cpp
  test_inmemory_interaction_repository.cpp
  test_null_embedding_index.cpp
  test_inmemory_embedding_index.cpp
)

target_link_libraries(ccmcp_tests
  PRIVATE
    ccmcp
    Catch2::Catch2WithMain
)

include(Catch)
catch_discover_tests(ccmcp_tests)
