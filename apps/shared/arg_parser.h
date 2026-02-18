#pragma once

#include <functional>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace ccmcp::apps {

// Option describes a single command-line flag accepted by an app or subcommand.
// Config is the caller-defined configuration struct that handlers populate.
//
// handler returns true on success, false on validation failure (the parser
// continues processing remaining flags regardless of the return value).
template <typename Config>
struct Option {
  std::string name;            // NOLINT(readability-identifier-naming)
  bool requires_value{false};  // NOLINT(readability-identifier-naming)
  std::string description;     // NOLINT(readability-identifier-naming)
  std::function<bool(Config&, const std::string& value)>
      handler;  // NOLINT(readability-identifier-naming)
};

// parse_options iterates argv[start..argc-1], dispatches each recognised flag
// to its handler, and returns the populated config.
// Unknown flags are reported to stderr. Unknown non-flag tokens are silently
// skipped (to allow callers to handle positional arguments separately).
template <typename Config>
Config parse_options(int argc, char* argv[],  // NOLINT(modernize-avoid-c-arrays)
                     const std::vector<Option<Config>>& options, int start = 1,
                     Config default_config = {}) {
  Config config = std::move(default_config);

  std::unordered_map<std::string, const Option<Config>*> option_map;
  for (const auto& opt : options) {
    option_map[opt.name] = &opt;
  }

  for (int i = start; i < argc; ++i) {
    std::string arg = argv[i];  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)

    auto it = option_map.find(arg);
    if (it != option_map.end()) {
      const Option<Config>* opt = it->second;
      if (opt->requires_value) {
        if (i + 1 < argc) {
          opt->handler(config,
                       argv[++i]);  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        } else {
          std::cerr << "Option " << arg << " requires a value\n";
        }
      } else {
        opt->handler(config, "");
      }
    } else if (!arg.empty() && arg[0] == '-') {
      // Only flag-like tokens are reported as unknown.
      std::cerr << "Unknown option: " << arg << "\n";
    }
  }

  return config;
}

}  // namespace ccmcp::apps
