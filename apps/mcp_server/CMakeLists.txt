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
