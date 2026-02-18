#pragma once

// cmd_ingest_resume: ingest a resume file into the SQLite database.
// Usage: ccmcp_cli ingest-resume <file-path> [--db <db-path>]
int cmd_ingest_resume(int argc, char* argv[]);  // NOLINT(modernize-avoid-c-arrays)
