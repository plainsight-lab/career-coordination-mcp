#include "ccmcp/ingest/hygiene.h"

#include <algorithm>
#include <sstream>
#include <string>

namespace ccmcp::ingest::hygiene {

std::string normalize_line_endings(const std::string& text) {
  std::string result;
  result.reserve(text.size());

  for (size_t i = 0; i < text.size(); ++i) {
    if (text[i] == '\r') {
      // Handle \r\n (Windows) and \r (old Mac)
      if (i + 1 < text.size() && text[i + 1] == '\n') {
        result += '\n';
        ++i;  // Skip the \n
      } else {
        result += '\n';
      }
    } else {
      result += text[i];
    }
  }

  return result;
}

std::string trim_trailing_whitespace(const std::string& text) {
  std::istringstream stream(text);
  std::string line;
  std::string result;

  while (std::getline(stream, line)) {
    // Find last non-whitespace character
    auto end = std::find_if(line.rbegin(), line.rend(), [](unsigned char ch) {
                 return !std::isspace(ch);
               }).base();
    result.append(line.begin(), end);
    result += '\n';
  }

  // Remove final newline if original text didn't end with one
  if (!text.empty() && text.back() != '\n' && text.back() != '\r') {
    if (!result.empty() && result.back() == '\n') {
      result.pop_back();
    }
  }

  return result;
}

std::string collapse_blank_lines(const std::string& text) {
  std::istringstream stream(text);
  std::string line;
  std::string result;
  int blank_count = 0;

  while (std::getline(stream, line)) {
    if (line.empty()) {
      ++blank_count;
      if (blank_count <= 2) {  // Allow up to 2 consecutive blank lines
        result += '\n';
      }
    } else {
      blank_count = 0;
      result += line;
      result += '\n';
    }
  }

  // Remove final newline if original text didn't end with one
  if (!text.empty() && text.back() != '\n' && text.back() != '\r') {
    if (!result.empty() && result.back() == '\n') {
      result.pop_back();
    }
  }

  return result;
}

std::string normalize_headings(const std::string& text) {
  // For v0.3, we'll keep this minimal - just ensure ATX headers are properly formatted
  // More aggressive normalization (setext -> ATX) deferred to avoid semantic changes
  std::istringstream stream(text);
  std::string line;
  std::string result;

  while (std::getline(stream, line)) {
    // Trim leading/trailing spaces around # in ATX headers
    if (!line.empty() && line[0] == '#') {
      size_t hash_end = line.find_first_not_of('#');
      if (hash_end != std::string::npos && hash_end > 0) {
        // Count hashes
        size_t hash_count = hash_end;
        // Extract text after hashes
        std::string rest = line.substr(hash_end);
        // Trim leading whitespace from text
        size_t text_start = rest.find_first_not_of(" \t");
        if (text_start != std::string::npos) {
          result += line.substr(0, hash_count);
          result += ' ';
          result += rest.substr(text_start);
        } else {
          result += line;  // Header with no text, keep as-is
        }
      } else {
        result += line;
      }
    } else {
      result += line;
    }
    result += '\n';
  }

  // Remove final newline if original text didn't end with one
  if (!text.empty() && text.back() != '\n' && text.back() != '\r') {
    if (!result.empty() && result.back() == '\n') {
      result.pop_back();
    }
  }

  return result;
}

std::string apply_hygiene(const std::string& text) {
  std::string result = text;
  result = normalize_line_endings(result);
  result = trim_trailing_whitespace(result);
  result = collapse_blank_lines(result);
  result = normalize_headings(result);
  return result;
}

}  // namespace ccmcp::ingest::hygiene
