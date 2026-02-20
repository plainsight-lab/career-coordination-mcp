add_library(mcp_transport_logic OBJECT
  method_handlers.cpp
  server_loop.cpp
  mcp_protocol.cpp
  handlers/match_opportunity.cpp
  handlers/validate_match_report.cpp
  handlers/get_audit_trace.cpp
  handlers/interaction_apply_event.cpp
  handlers/tool_registry.cpp
  handlers/ingest_resume.cpp
  handlers/index_build.cpp
  handlers/get_decision.cpp
)

target_link_libraries(mcp_transport_logic PRIVATE ccmcp)

# ${CMAKE_CURRENT_SOURCE_DIR}/..  → apps/ (for "shared/arg_parser.h" in config.cpp)
target_include_directories(mcp_transport_logic PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_compile_definitions(mcp_transport_logic PRIVATE CCMCP_TRANSPORT_BOUNDARY_GUARD)

add_executable(mcp_server
  main.cpp
  config.cpp
  startup_guard.cpp
  $<TARGET_OBJECTS:mcp_transport_logic>
)

target_link_libraries(mcp_server
  PRIVATE
    ccmcp
)

# ${CMAKE_CURRENT_SOURCE_DIR}/..  → apps/ (for "shared/arg_parser.h" in config.cpp)
target_include_directories(mcp_server PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/..
)
