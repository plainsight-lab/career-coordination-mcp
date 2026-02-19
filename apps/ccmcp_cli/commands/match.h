#pragma once

// cmd_match: run a demo match against a hardcoded ExampleCo opportunity.
// Usage: ccmcp_cli match [--db <db-path>] [--matching-strategy lexical|hybrid]
//                        [--vector-backend inmemory|sqlite] [--vector-db-path <dir>]
//                        [--override-rule <rule_id> --operator <id> --reason "<text>"]
// Override flags are all-or-nothing: providing a partial set is a usage error.
int cmd_match(int argc, char* argv[]);  // NOLINT(modernize-avoid-c-arrays)
