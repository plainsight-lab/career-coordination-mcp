#include "ccmcp/ingest/format_adapter.h"

#include <algorithm>
#include <pugixml.hpp>
#include <sstream>
#include <string>
#include <vector>
#include <zip.h>

namespace ccmcp::ingest {

ExtractionResult MarkdownAdapter::extract(const std::vector<uint8_t>& data) const {
  if (data.empty()) {
    return ExtractionResult::err(ExtractionError{"Empty input data"});
  }

  // Convert bytes to UTF-8 string (assuming input is UTF-8)
  std::string markdown(data.begin(), data.end());
  return ExtractionResult::ok(markdown);
}

ExtractionResult TextAdapter::extract(const std::vector<uint8_t>& data) const {
  if (data.empty()) {
    return ExtractionResult::err(ExtractionError{"Empty input data"});
  }

  // Convert bytes to UTF-8 string
  std::string text(data.begin(), data.end());

  // Wrap in markdown (simple approach: preserve as-is, or use code fence for formatting)
  // For v0.3, we'll preserve the text as-is with minimal wrapping
  std::string markdown = "# Resume\n\n" + text;

  return ExtractionResult::ok(markdown);
}

namespace {

// Helper to extract text strings from PDF content stream
std::string extract_text_from_pdf_content(const std::string& content) {
  std::ostringstream text;
  size_t pos = 0;

  // Look for text showing operators: Tj, TJ, '
  while (pos < content.size()) {
    // Find text in parentheses (string literals in PDF)
    size_t paren_start = content.find('(', pos);
    if (paren_start == std::string::npos) {
      break;
    }

    // Find matching close paren (handle escaped parens)
    size_t paren_end = paren_start + 1;
    int depth = 1;
    while (paren_end < content.size() && depth > 0) {
      if (content[paren_end] == '\\' && paren_end + 1 < content.size()) {
        paren_end += 2;  // Skip escaped character
        continue;
      }
      if (content[paren_end] == '(') {
        depth++;
      } else if (content[paren_end] == ')') {
        depth--;
      }
      paren_end++;
    }

    if (depth == 0) {
      // Extract text between parens
      std::string pdf_string = content.substr(paren_start + 1, paren_end - paren_start - 2);

      // Basic unescape (handle common cases)
      std::string unescaped;
      for (size_t i = 0; i < pdf_string.size(); ++i) {
        if (pdf_string[i] == '\\' && i + 1 < pdf_string.size()) {
          char next = pdf_string[i + 1];
          if (next == 'n') {
            unescaped += '\n';
            i++;
          } else if (next == 'r') {
            unescaped += '\r';
            i++;
          } else if (next == 't') {
            unescaped += '\t';
            i++;
          } else if (next == '\\' || next == '(' || next == ')') {
            unescaped += next;
            i++;
          } else {
            unescaped += pdf_string[i];
          }
        } else {
          unescaped += pdf_string[i];
        }
      }

      text << unescaped << " ";
      pos = paren_end;
    } else {
      pos = paren_start + 1;
    }
  }

  return text.str();
}

}  // namespace

ExtractionResult PdfAdapter::extract(const std::vector<uint8_t>& data) const {
  if (data.empty()) {
    return ExtractionResult::err(ExtractionError{"Empty input data"});
  }

  // Verify PDF header
  std::string pdf_data(data.begin(), data.end());
  if (pdf_data.size() < 5 || pdf_data.substr(0, 4) != "%PDF") {
    return ExtractionResult::err(ExtractionError{"Invalid PDF: missing %PDF header"});
  }

  // Extract text from content streams
  // Look for stream objects containing text operators
  std::ostringstream extracted_text;
  extracted_text << "# Resume\n\n";

  size_t pos = 0;
  while (pos < pdf_data.size()) {
    // Find stream keyword
    size_t stream_start = pdf_data.find("\nstream\n", pos);
    if (stream_start == std::string::npos) {
      stream_start = pdf_data.find("\rstream\r", pos);
    }
    if (stream_start == std::string::npos) {
      break;
    }

    // Find endstream keyword
    size_t stream_end = pdf_data.find("\nendstream", stream_start);
    if (stream_end == std::string::npos) {
      stream_end = pdf_data.find("\rendstream", stream_start);
    }
    if (stream_end == std::string::npos) {
      break;
    }

    // Extract content stream
    std::string stream_content = pdf_data.substr(stream_start + 8,  // Skip "\nstream\n"
                                                 stream_end - stream_start - 8);

    // Extract text from this content stream
    std::string text = extract_text_from_pdf_content(stream_content);
    if (!text.empty()) {
      extracted_text << text;
    }

    pos = stream_end + 10;  // Skip "\nendstream"
  }

  std::string result = extracted_text.str();
  if (result == "# Resume\n\n") {
    return ExtractionResult::err(ExtractionError{"No text content found in PDF"});
  }

  return ExtractionResult::ok(result);
}

ExtractionResult DocxAdapter::extract(const std::vector<uint8_t>& data) const {
  if (data.empty()) {
    return ExtractionResult::err(ExtractionError{"Empty input data"});
  }

  // Create an in-memory ZIP archive from the DOCX data
  zip_error_t error;
  zip_source_t* src = zip_source_buffer_create(data.data(), data.size(), 0, &error);
  if (!src) {
    return ExtractionResult::err(ExtractionError{"Failed to create ZIP source from DOCX data"});
  }

  zip_t* archive = zip_open_from_source(src, ZIP_RDONLY, &error);
  if (!archive) {
    zip_source_free(src);
    return ExtractionResult::err(ExtractionError{"Failed to open DOCX as ZIP archive"});
  }

  // Read word/document.xml from the ZIP
  zip_stat_t st;
  if (zip_stat(archive, "word/document.xml", 0, &st) != 0) {
    zip_close(archive);
    return ExtractionResult::err(ExtractionError{"Failed to find word/document.xml in DOCX"});
  }

  zip_file_t* file = zip_fopen(archive, "word/document.xml", 0);
  if (!file) {
    zip_close(archive);
    return ExtractionResult::err(ExtractionError{"Failed to open word/document.xml"});
  }

  std::vector<char> xml_data(st.size);
  zip_int64_t bytes_read = zip_fread(file, xml_data.data(), st.size);
  zip_fclose(file);
  zip_close(archive);

  if (bytes_read != static_cast<zip_int64_t>(st.size)) {
    return ExtractionResult::err(ExtractionError{"Failed to read word/document.xml completely"});
  }

  // Parse XML and extract text
  pugi::xml_document doc;
  pugi::xml_parse_result result = doc.load_buffer(xml_data.data(), xml_data.size());
  if (!result) {
    return ExtractionResult::err(ExtractionError{"Failed to parse word/document.xml as XML"});
  }

  // Extract text from <w:t> elements, preserving paragraph structure
  std::ostringstream text_stream;
  text_stream << "# Resume\n\n";

  // Iterate through paragraphs (<w:p> elements)
  pugi::xpath_node_set paragraphs = doc.select_nodes("//w:p");
  for (size_t i = 0; i < paragraphs.size(); ++i) {
    pugi::xml_node paragraph = paragraphs[i].node();
    bool paragraph_has_text = false;

    // Extract text from all <w:t> elements in this paragraph
    pugi::xpath_node_set text_nodes = paragraph.select_nodes(".//w:t");
    for (size_t j = 0; j < text_nodes.size(); ++j) {
      pugi::xml_node text_node = text_nodes[j].node();
      std::string text_content = text_node.child_value();
      if (!text_content.empty()) {
        text_stream << text_content;
        paragraph_has_text = true;
      }
    }

    // Add newline after paragraph if it had text
    if (paragraph_has_text) {
      text_stream << "\n";
    }
  }

  return ExtractionResult::ok(text_stream.str());
}

}  // namespace ccmcp::ingest
