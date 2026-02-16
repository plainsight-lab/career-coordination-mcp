cmake_minimum_required(VERSION 3.21)

project(career-coordination-mcp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile_commands.json for clang-tidy
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Optional: Enable clang-tidy during build
# Uncomment to run clang-tidy automatically on every build
# set(CMAKE_CXX_CLANG_TIDY clang-tidy --config-file=${CMAKE_SOURCE_DIR}/.clang-tidy)

find_package(fmt CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(Catch2 CONFIG REQUIRED)
find_package(unofficial-sqlite3 CONFIG REQUIRED)

add_library(ccmcp
  src/core/hashing.cpp
  src/core/id_generator.cpp
  src/core/clock.cpp
  src/domain/experience_atom.cpp
  src/domain/requirement.cpp
  src/domain/opportunity.cpp
  src/domain/contact.cpp
  src/domain/interaction.cpp
  src/domain/resume.cpp
  src/domain/match_report.cpp
  src/constitution/validation_engine.cpp
  src/constitution/rules/schema_001.cpp
  src/constitution/rules/evid_001.cpp
  src/constitution/rules/score_001.cpp
  src/storage/audit_log.cpp
  src/storage/inmemory_atom_repository.cpp
  src/storage/inmemory_opportunity_repository.cpp
  src/storage/inmemory_interaction_repository.cpp
  src/storage/sqlite/sqlite_db.cpp
  src/storage/sqlite/sqlite_atom_repository.cpp
  src/storage/sqlite/sqlite_opportunity_repository.cpp
  src/storage/sqlite/sqlite_interaction_repository.cpp
  src/storage/sqlite/sqlite_audit_log.cpp
  src/embedding/deterministic_stub_embedding_provider.cpp
  src/vector/null_embedding_index.cpp
  src/vector/inmemory_embedding_index.cpp
  src/vector/lancedb_embedding_index.cpp
  src/matching/matcher.cpp
)

target_include_directories(ccmcp
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(ccmcp
  PUBLIC
    fmt::fmt
    nlohmann_json::nlohmann_json
    unofficial::sqlite3::sqlite3
)

if(MSVC)
  target_compile_options(ccmcp PRIVATE /W4 /permissive-)
else()
  target_compile_options(ccmcp PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
endif()

add_subdirectory(apps/ccmcp_cli)

enable_testing()
add_subdirectory(tests)
