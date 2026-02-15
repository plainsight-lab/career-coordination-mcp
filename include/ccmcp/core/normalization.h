#pragma once

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

namespace ccmcp::core {

// Deterministic ASCII-only normalization utilities for v0.1.
// These functions are locale-independent and produce byte-stable output
// across all platforms and compilers.
//
// LOCKED SPEC (v0.1):
// - ASCII lowercasing: A-Z → a-z via explicit char math (no std::tolower)
// - Non-alphanumeric → space delimiter
// - Token length minimum: 2 characters (configurable)
// - Deduplication and sorting for tags (lexicographic)
// - No locale dependence, no undefined behavior

// normalize_ascii_lower converts ASCII uppercase (A-Z) to lowercase (a-z).
// Non-ASCII characters are preserved unchanged.
// This is deterministic and locale-independent.
inline std::string normalize_ascii_lower(const std::string_view input) {
  std::string result;
  result.reserve(input.size());

  for (const char ch : input) {
    // ES.46: Avoid lossy conversions - explicit range check
    if (ch >= 'A' && ch <= 'Z') {
      // Per.11: Move computation to compile time when possible
      constexpr char kCaseOffset = 'a' - 'A';
      result.push_back(static_cast<char>(ch + kCaseOffset));
    } else {
      result.push_back(ch);
    }
  }

  return result;
}

// tokenize_ascii splits input on non-alphanumeric delimiters into tokens.
// - Converts to lowercase
// - Replaces non-alphanumeric with space
// - Splits on whitespace
// - Drops tokens shorter than min_length
// - Returns tokens in encounter order (caller sorts if needed)
inline std::vector<std::string> tokenize_ascii(const std::string_view input,
                                               const std::size_t min_length = 2) {
  // First pass: normalize to lowercase and replace non-alnum with space
  std::string normalized;
  normalized.reserve(input.size());

  for (const char ch : input) {
    if ((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9')) {
      normalized.push_back(ch);
    } else if (ch >= 'A' && ch <= 'Z') {
      constexpr char kCaseOffset = 'a' - 'A';
      normalized.push_back(static_cast<char>(ch + kCaseOffset));
    } else {
      // Replace any non-alphanumeric with space delimiter
      normalized.push_back(' ');
    }
  }

  // Second pass: split on whitespace into tokens
  std::vector<std::string> tokens;
  std::string current_token;
  current_token.reserve(32);  // Reasonable default to reduce allocations

  for (const char ch : normalized) {
    if (ch == ' ') {
      if (!current_token.empty() && current_token.size() >= min_length) {
        tokens.push_back(std::move(current_token));
        current_token.clear();
        current_token.reserve(32);
      } else {
        current_token.clear();
      }
    } else {
      current_token.push_back(ch);
    }
  }

  // Handle final token
  if (!current_token.empty() && current_token.size() >= min_length) {
    tokens.push_back(std::move(current_token));
  }

  return tokens;
}

// normalize_tags performs full tag normalization:
// - Tokenizes input tags
// - Deduplicates
// - Sorts lexicographically
// - Returns stable, deterministic tag list
inline std::vector<std::string> normalize_tags(const std::vector<std::string>& input_tags) {
  std::vector<std::string> all_tokens;

  // Tokenize each tag
  for (const auto& tag : input_tags) {
    auto tokens = tokenize_ascii(tag);
    all_tokens.insert(all_tokens.end(), std::make_move_iterator(tokens.begin()),
                      std::make_move_iterator(tokens.end()));
  }

  // Sort for deduplication
  std::sort(all_tokens.begin(), all_tokens.end());

  // Deduplicate (std::unique moves duplicates to end, returns new end iterator)
  auto unique_end = std::unique(all_tokens.begin(), all_tokens.end());
  all_tokens.erase(unique_end, all_tokens.end());

  return all_tokens;
}

// trim removes leading and trailing whitespace (ASCII space/tab/newline)
inline std::string trim(const std::string_view input) {
  if (input.empty()) {
    return std::string{};
  }

  // Find first non-whitespace
  std::size_t start = 0;
  while (start < input.size() && (input[start] == ' ' || input[start] == '\t' ||
                                  input[start] == '\n' || input[start] == '\r')) {
    ++start;
  }

  // All whitespace
  if (start == input.size()) {
    return std::string{};
  }

  // Find last non-whitespace
  std::size_t end = input.size();
  while (end > start && (input[end - 1] == ' ' || input[end - 1] == '\t' ||
                         input[end - 1] == '\n' || input[end - 1] == '\r')) {
    --end;
  }

  return std::string{input.substr(start, end - start)};
}

}  // namespace ccmcp::core
