add_executable(ccmcp_cli
  main.cpp
  commands/ingest_resume.cpp
  commands/tokenize_resume.cpp
  commands/index_build.cpp
  commands/match.cpp
)

target_link_libraries(ccmcp_cli PRIVATE ccmcp nlohmann_json::nlohmann_json)

# ${CMAKE_CURRENT_SOURCE_DIR}     → apps/ccmcp_cli/ (for "commands/*.h" includes from main.cpp)
# ${CMAKE_CURRENT_SOURCE_DIR}/..  → apps/           (for "shared/arg_parser.h" in command files)
target_include_directories(ccmcp_cli PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/..
)
