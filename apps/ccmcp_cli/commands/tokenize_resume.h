#pragma once

// cmd_tokenize_resume: tokenize an ingested resume and store the token IR.
// Usage: ccmcp_cli tokenize-resume <resume-id> [--db <db-path>]
//                                              [--mode <deterministic|stub-inference>]
int cmd_tokenize_resume(int argc, char* argv[]);  // NOLINT(modernize-avoid-c-arrays)
