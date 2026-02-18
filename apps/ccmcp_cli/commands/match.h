#pragma once

// cmd_match: run a demo match against a hardcoded ExampleCo opportunity.
// Usage: ccmcp_cli match [--db <db-path>] [--matching-strategy lexical|hybrid]
//                        [--vector-backend inmemory|sqlite] [--vector-db-path <dir>]
int cmd_match(int argc, char* argv[]);  // NOLINT(modernize-avoid-c-arrays)
