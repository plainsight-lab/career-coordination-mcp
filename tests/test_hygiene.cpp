#include "ccmcp/ingest/hygiene.h"

#include <catch2/catch_test_macros.hpp>

using namespace ccmcp::ingest::hygiene;

TEST_CASE("normalize_line_endings converts CRLF to LF", "[ingest][hygiene]") {
  std::string input = "Line 1\r\nLine 2\r\nLine 3";
  std::string result = normalize_line_endings(input);
  REQUIRE(result == "Line 1\nLine 2\nLine 3");
}

TEST_CASE("normalize_line_endings converts CR to LF", "[ingest][hygiene]") {
  std::string input = "Line 1\rLine 2\rLine 3";
  std::string result = normalize_line_endings(input);
  REQUIRE(result == "Line 1\nLine 2\nLine 3");
}

TEST_CASE("normalize_line_endings preserves LF", "[ingest][hygiene]") {
  std::string input = "Line 1\nLine 2\nLine 3";
  std::string result = normalize_line_endings(input);
  REQUIRE(result == input);
}

TEST_CASE("trim_trailing_whitespace removes spaces and tabs", "[ingest][hygiene]") {
  std::string input = "Line 1   \nLine 2\t\t\nLine 3";
  std::string result = trim_trailing_whitespace(input);
  REQUIRE(result == "Line 1\nLine 2\nLine 3");
}

TEST_CASE("trim_trailing_whitespace preserves leading whitespace", "[ingest][hygiene]") {
  std::string input = "  Line 1\n\tLine 2";
  std::string result = trim_trailing_whitespace(input);
  REQUIRE(result == "  Line 1\n\tLine 2");
}

TEST_CASE("collapse_blank_lines limits to 2 consecutive blanks", "[ingest][hygiene]") {
  std::string input = "Line 1\n\n\n\n\nLine 2";
  std::string result = collapse_blank_lines(input);
  REQUIRE(result == "Line 1\n\n\nLine 2");
}

TEST_CASE("collapse_blank_lines preserves single and double blanks", "[ingest][hygiene]") {
  std::string input = "Line 1\n\nLine 2\n\n\nLine 3";
  std::string result = collapse_blank_lines(input);
  REQUIRE(result == "Line 1\n\nLine 2\n\n\nLine 3");
}

TEST_CASE("normalize_headings ensures space after hash", "[ingest][hygiene]") {
  std::string input = "#Heading 1\n##Heading 2";
  std::string result = normalize_headings(input);
  REQUIRE(result == "# Heading 1\n## Heading 2");
}

TEST_CASE("normalize_headings preserves properly formatted headings", "[ingest][hygiene]") {
  std::string input = "# Heading 1\n## Heading 2";
  std::string result = normalize_headings(input);
  REQUIRE(result == input);
}

TEST_CASE("apply_hygiene applies full pipeline", "[ingest][hygiene]") {
  std::string input = "#Resume  \r\n\r\n\r\n\r\nExperience   \r\n";
  std::string result = apply_hygiene(input);

  // Should normalize line endings, trim trailing whitespace, collapse blanks, and normalize
  // headings Note: Input ends with \r\n, so output ends with \n (preserved)
  REQUIRE(result == "# Resume\n\n\nExperience\n");
}

TEST_CASE("apply_hygiene is deterministic", "[ingest][hygiene]") {
  std::string input = "# Resume\r\n\r\nExperience\t\r\n";
  std::string result1 = apply_hygiene(input);
  std::string result2 = apply_hygiene(input);
  REQUIRE(result1 == result2);
}
