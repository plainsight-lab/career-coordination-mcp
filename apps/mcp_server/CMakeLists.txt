add_executable(mcp_server
  main.cpp
  mcp_protocol.cpp
)

target_link_libraries(mcp_server
  PRIVATE
    ccmcp
)
