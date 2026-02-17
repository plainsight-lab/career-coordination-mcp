#pragma once

#include <string>

namespace ccmcp::ingest {

/// Deterministic hygiene normalization for resume markdown
namespace hygiene {

/// Normalize line endings to \n (Unix style)
std::string normalize_line_endings(const std::string& text);

/// Trim trailing whitespace from each line
std::string trim_trailing_whitespace(const std::string& text);

/// Collapse repeated blank lines (3+ blanks → 2 blanks max)
std::string collapse_blank_lines(const std::string& text);

/// Normalize markdown headings where safe (ensure # prefix for ATX headers)
std::string normalize_headings(const std::string& text);

/// Apply full hygiene pipeline (all normalization steps)
std::string apply_hygiene(const std::string& text);

}  // namespace hygiene
}  // namespace ccmcp::ingest
