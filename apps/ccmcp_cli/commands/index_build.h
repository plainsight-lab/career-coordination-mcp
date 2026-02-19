#pragma once

// cmd_index_build: build or rebuild the embedding vector index.
// Usage: ccmcp_cli index-build [--db <path>] [--vector-backend inmemory|sqlite]
//                              [--vector-db-path <dir>]  (required when --vector-backend sqlite)
//                              [--scope atoms|resumes|opportunities|all]
int cmd_index_build(int argc, char* argv[]);  // NOLINT(modernize-avoid-c-arrays)
