add_executable(ccmcp_tests
  test_ids.cpp
  test_validation_engine.cpp
  test_matching_stub.cpp
  test_domain_schema.cpp
)

target_link_libraries(ccmcp_tests
  PRIVATE
    ccmcp
    Catch2::Catch2WithMain
)

include(Catch)
catch_discover_tests(ccmcp_tests)
