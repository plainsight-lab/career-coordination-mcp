#include "ccmcp/ingest/format_adapter.h"

#include <catch2/catch_test_macros.hpp>

#include <vector>

using namespace ccmcp::ingest;

TEST_CASE("MarkdownAdapter pass-through extraction", "[ingest][format_adapter]") {
  MarkdownAdapter adapter;

  std::string input = "# Resume\n\nExperience at Tech Corp";
  std::vector<uint8_t> data(input.begin(), input.end());

  auto result = adapter.extract(data);

  REQUIRE(result.has_value());
  REQUIRE(result.value() == input);
  REQUIRE(adapter.extraction_method() == "md-pass-through-v1");
}

TEST_CASE("MarkdownAdapter rejects empty input", "[ingest][format_adapter]") {
  MarkdownAdapter adapter;

  std::vector<uint8_t> empty_data;
  auto result = adapter.extract(empty_data);

  REQUIRE(!result.has_value());
  REQUIRE(result.error().message == "Empty input data");
}

TEST_CASE("TextAdapter wraps plain text in markdown", "[ingest][format_adapter]") {
  TextAdapter adapter;

  std::string input = "John Doe\nSoftware Engineer\nTech Corp";
  std::vector<uint8_t> data(input.begin(), input.end());

  auto result = adapter.extract(data);

  REQUIRE(result.has_value());
  REQUIRE(result.value().starts_with("# Resume\n\n"));
  REQUIRE(result.value().find("John Doe") != std::string::npos);
  REQUIRE(adapter.extraction_method() == "txt-wrap-v1");
}

TEST_CASE("TextAdapter rejects empty input", "[ingest][format_adapter]") {
  TextAdapter adapter;

  std::vector<uint8_t> empty_data;
  auto result = adapter.extract(empty_data);

  REQUIRE(!result.has_value());
  REQUIRE(result.error().message == "Empty input data");
}

TEST_CASE("PdfAdapter extracts text from simple PDF", "[ingest][format_adapter]") {
  PdfAdapter adapter;

  // Create a minimal valid PDF with text content
  std::string pdf_content = R"(%PDF-1.4
1 0 obj
<< /Type /Catalog /Pages 2 0 R >>
endobj
2 0 obj
<< /Type /Pages /Kids [3 0 R] /Count 1 >>
endobj
3 0 obj
<< /Type /Page /Parent 2 0 R /Contents 4 0 R >>
endobj
4 0 obj
<< /Length 44 >>
stream
BT
/F1 12 Tf
100 700 Td
(Software Engineer) Tj
ET
endstream
endobj
xref
0 5
0000000000 65535 f
0000000009 00000 n
0000000058 00000 n
0000000115 00000 n
0000000184 00000 n
trailer
<< /Size 5 /Root 1 0 R >>
startxref
282
%%EOF
)";

  std::vector<uint8_t> data(pdf_content.begin(), pdf_content.end());
  auto result = adapter.extract(data);

  REQUIRE(result.has_value());
  REQUIRE(result.value().find("Software Engineer") != std::string::npos);
  REQUIRE(adapter.extraction_method() == "pdf-text-extract-v1");
}

TEST_CASE("PdfAdapter rejects invalid PDF", "[ingest][format_adapter]") {
  PdfAdapter adapter;

  std::vector<uint8_t> invalid_data{0x00, 0x01, 0x02, 0x03};
  auto result = adapter.extract(invalid_data);

  REQUIRE(!result.has_value());
  REQUIRE(result.error().message.find("Invalid PDF") != std::string::npos);
}

TEST_CASE("DocxAdapter handles DOCX structure errors gracefully", "[ingest][format_adapter]") {
  DocxAdapter adapter;

  // Invalid DOCX (not a valid ZIP)
  std::vector<uint8_t> invalid_data{0x00, 0x01, 0x02, 0x03};
  auto result = adapter.extract(invalid_data);

  REQUIRE(!result.has_value());
  REQUIRE(result.error().message.find("ZIP") != std::string::npos);
  REQUIRE(adapter.extraction_method() == "docx-extract-v1");
}
