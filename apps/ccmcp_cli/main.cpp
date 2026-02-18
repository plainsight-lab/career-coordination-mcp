#include "commands/index_build.h"
#include "commands/ingest_resume.h"
#include "commands/match.h"
#include "commands/tokenize_resume.h"
#include <array>
#include <iostream>
#include <string_view>

namespace {

struct Command {
  std::string_view name;         // NOLINT(readability-identifier-naming)
  std::string_view description;  // NOLINT(readability-identifier-naming)
  int (*handler)(int,
                 char*[]);  // NOLINT(readability-identifier-naming,modernize-avoid-c-arrays)
};

const std::array<Command, 4> kCommands = {{
    {"ingest-resume", "Ingest a resume file into the database", cmd_ingest_resume},
    {"tokenize-resume", "Tokenize an ingested resume into a token IR", cmd_tokenize_resume},
    {"index-build", "Build or rebuild the embedding vector index", cmd_index_build},
    {"match", "Run a demo match against a hardcoded ExampleCo opportunity", cmd_match},
}};

void print_usage(const char* prog) {
  std::cerr << "Usage: " << prog << " <command> [options]\n\nCommands:\n";
  for (const auto& cmd : kCommands) {
    std::cerr << "  " << cmd.name << "\n    " << cmd.description << "\n";
  }
}

}  // namespace

int main(int argc, char* argv[]) {  // NOLINT(modernize-avoid-c-arrays)
  if (argc < 2) {
    print_usage(argv[0]);  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    return 1;
  }

  const std::string_view subcommand =
      argv[1];  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  for (const auto& cmd : kCommands) {
    if (cmd.name == subcommand) {
      return cmd.handler(argc, argv);
    }
  }

  std::cerr << "Unknown command: " << subcommand << "\n\n";
  print_usage(argv[0]);  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  return 1;
}
