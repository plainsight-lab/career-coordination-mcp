add_library(ccmcp_cli_logic OBJECT
  commands/ingest_resume_logic.cpp
  commands/tokenize_resume_logic.cpp
  commands/index_build_logic.cpp
  commands/match_logic.cpp
  commands/decision_logic.cpp
)

target_link_libraries(ccmcp_cli_logic PRIVATE ccmcp nlohmann_json::nlohmann_json)

# ${CMAKE_CURRENT_SOURCE_DIR}     → apps/ccmcp_cli/ (for "commands/*.h" relative includes)
# ${CMAKE_CURRENT_SOURCE_DIR}/..  → apps/           (for "shared/arg_parser.h" in logic files)
target_include_directories(ccmcp_cli_logic PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_compile_definitions(ccmcp_cli_logic PRIVATE CCMCP_TRANSPORT_BOUNDARY_GUARD)

add_executable(ccmcp_cli
  main.cpp
  commands/ingest_resume.cpp
  commands/tokenize_resume.cpp
  commands/index_build.cpp
  commands/match.cpp
  commands/decision.cpp
  commands/redis_health.cpp
  $<TARGET_OBJECTS:ccmcp_cli_logic>
)

target_link_libraries(ccmcp_cli PRIVATE ccmcp nlohmann_json::nlohmann_json)

# ${CMAKE_CURRENT_SOURCE_DIR}     → apps/ccmcp_cli/ (for "commands/*.h" includes from main.cpp)
# ${CMAKE_CURRENT_SOURCE_DIR}/..  → apps/           (for "shared/arg_parser.h" in command files)
target_include_directories(ccmcp_cli PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/..
)
