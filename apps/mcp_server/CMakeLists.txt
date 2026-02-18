add_executable(mcp_server
  main.cpp
  mcp_protocol.cpp
  config.cpp
  server_loop.cpp
  method_handlers.cpp
  handlers/match_opportunity.cpp
  handlers/validate_match_report.cpp
  handlers/get_audit_trace.cpp
  handlers/interaction_apply_event.cpp
  handlers/tool_registry.cpp
)

target_link_libraries(mcp_server
  PRIVATE
    ccmcp
)

# ${CMAKE_CURRENT_SOURCE_DIR}/..  → apps/ (for "shared/arg_parser.h" in config.cpp)
target_include_directories(mcp_server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
